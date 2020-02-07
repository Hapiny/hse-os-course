#include <inc/stab.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/dwarf.h>

#include <kern/kdebug.h>

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
