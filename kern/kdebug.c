#include <inc/stab.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/dwarf.h>

#include <kern/kdebug.h>

extern const struct Stab __STAB_BEGIN__[];	// Beginning of stabs table
extern const struct Stab __STAB_END__[];	// End of stabs table
extern const char __STABSTR_BEGIN__[];		// Beginning of string table
extern const char __STABSTR_END__[];		// End of string table


// stab_binsearch(stabs, region_left, region_right, type, addr)
//
//	Some stab types are arranged in increasing order by instruction
//	address.  For example, N_FUN stabs (stab entries with n_type ==
//	N_FUN), which mark functions, and N_SO stabs, which mark source files.
//
//	Given an instruction address, this function finds the single stab
//	entry of type 'type' that contains that address.
//
//	The search takes place within the range [*region_left, *region_right].
//	Thus, to search an entire set of N stabs, you might do:
//
//		left = 0;
//		right = N - 1;     /* rightmost stab */
//		stab_binsearch(stabs, &left, &right, type, addr);
//
//	The search modifies *region_left and *region_right to bracket the
//	'addr'.  *region_left points to the matching stab that contains
//	'addr', and *region_right points just before the next stab.  If
//	*region_left > *region_right, then 'addr' is not contained in any
//	matching stab.
//
//	For example, given these N_SO stabs:
//		Index  Type   Address
//		0      SO     f0100000
//		13     SO     f0100040
//		117    SO     f0100176
//		118    SO     f0100178
//		555    SO     f0100652
//		556    SO     f0100654
//		657    SO     f0100849
//	this code:
//		left = 0, right = 657;
//		stab_binsearch(stabs, &left, &right, N_SO, 0xf0100184);
//	will exit setting left = 118, right = 554.
//
static void
stab_binsearch(const struct Stab *stabs, int *region_left, int *region_right,
	       int type, uintptr_t addr)
{
	int l = *region_left, r = *region_right, any_matches = 0;

	while (l <= r) {
		int true_m = (l + r) / 2, m = true_m;

		// search for earliest stab with right type
		while (m >= l && stabs[m].n_type != type)
			m--;
		if (m < l) {	// no match in [l, m]
			l = true_m + 1;
			continue;
		}

		// actual binary search
		any_matches = 1;
		if (stabs[m].n_value < addr) {
			*region_left = m;
			l = true_m + 1;
		} else if (stabs[m].n_value > addr) {
			*region_right = m - 1;
			r = m - 1;
		} else {
			// exact match for 'addr', but continue loop to find
			// *region_right
			*region_left = m;
			l = m;
			addr++;
		}
	}

	if (!any_matches)
		*region_right = *region_left - 1;
	else {
		// find rightmost region containing 'addr'
		for (l = *region_right;
		     l > *region_left && stabs[l].n_type != type;
		     l--)
			/* do nothing */;
		*region_left = l;
	}
}

uintptr_t
stabs_find_function(const char * const fname)
{
	// const struct Stab *stabs = __STAB_BEGIN__, *stab_end = __STAB_END__;
	// const char *stabstr = __STABSTR_BEGIN__, *stabstr_end = __STABSTR_END__;

	//LAB 3: Your code here.
	return 0;
}

// debuginfo_eip(addr, info)
//
//	Fill in the 'info' structure with information about the specified
//	instruction address, 'addr'.  Returns 0 if information was found, and
//	negative if not.  But even if it returns negative it has stored some
//	information into '*info'.
//
int
debuginfo_eip(uintptr_t addr, struct Eipdebuginfo *info)
{
	// Initialize *info
	info->eip_file = "<unknown>";
	info->eip_line = 0;
	info->eip_fn_name = "<unknown>";
	info->eip_fn_namelen = 9;
	info->eip_fn_addr = addr;
	info->eip_fn_narg = 0;

	struct Dwarf_Addrs addrs;
	if (addr >= ULIM) {
		panic("Can't search for user-level addresses yet!");
	} else {
		addrs.abbrev_begin = __DEBUG_ABBREV_BEGIN__;
		addrs.abbrev_end = __DEBUG_ABBREV_END__;
		addrs.aranges_begin = __DEBUG_ARANGES_BEGIN__;
		addrs.aranges_end = __DEBUG_ARANGES_END__;
		addrs.info_begin = __DEBUG_INFO_BEGIN__;
		addrs.info_end = __DEBUG_INFO_END__;
		addrs.line_begin = __DEBUG_LINE_BEGIN__;
		addrs.line_end = __DEBUG_LINE_END__;
		addrs.str_begin = __DEBUG_STR_BEGIN__;
		addrs.str_end = __DEBUG_STR_END__;
		addrs.pubnames_begin = __DEBUG_PUBNAMES_BEGIN__;
		addrs.pubnames_end = __DEBUG_PUBNAMES_END__;
		addrs.pubtypes_begin = __DEBUG_PUBTYPES_BEGIN__;
		addrs.pubtypes_end = __DEBUG_PUBTYPES_END__;
	}
	enum {
	      BUFSIZE = 20,
	};
	Dwarf_Off offset = 0, line_offset = 0;
	int code = info_by_address(&addrs, addr, &offset);
	if (code < 0) {
		return code;
	}
	void *buf;
	buf = &info->eip_file;
	code = file_name_by_info(&addrs, offset, buf, sizeof(char*), &line_offset);
	if (code < 0) {
		return code;
	}
	// Find line number corresponding to given address.
	// Hint: note that we need the address of `call` instruction, but eip holds
	// address of the next instruction, so we should substract 5 from it.
	// Hint: use line_for_address from kern/dwarf_lines.c
	// Your code here:

	buf = &info->eip_fn_name;
	code = function_by_info(&addrs, addr, offset, buf, sizeof(char *), &info->eip_fn_addr);
	info->eip_fn_namelen = strlen(info->eip_fn_name);
	if (code < 0) {
		return code;
	}
	return 0;
}

uintptr_t
dwarf_find_function(const char * const fname)
{
	// There are two functions for function name lookup.
	// address_by_fname, which looks for function name in section .debug_pubnames
	// and naive_address_by_fname which performs full traversal of DIE tree.
	// LAB3: Your code here
	return 0;
}

uintptr_t
find_function(const char *const fname)
{
	uintptr_t address = stabs_find_function(fname);
	if (address == 0) {
		address = dwarf_find_function(fname);
	}
	return address;
}
