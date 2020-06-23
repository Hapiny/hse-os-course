#include <inc/assert.h>
#include <inc/x86.h>
#include <kern/env.h>
#include <kern/monitor.h>


struct Taskstate cpu_ts;
void sched_halt(void);

// Choose a user environment to run and run it.
void
sched_yield(void)
{
	// Implement simple round-robin scheduling.
	//
	// Search through 'envs' for an ENV_RUNNABLE environment in
	// circular fashion starting just after the env was
	// last running.  Switch to the first such environment found.
	//
	// If no envs are runnable, but the environment previously
	// running is still ENV_RUNNING, it's okay to
	// choose that environment.
	//
	// If there are no runnable environments,
	// simply drop through to the code
	// below to halt the cpu.

	struct Env *env;
	// Берем следующий ENV
	// Если curenv - NULL (в самом начале никто не запущен), то идем с начала массива envs
	// Иначе берем следующий из массива с учетом того, что текущий может последним, 
	// поэтому берем по модулю
	if (curenv) {
		env = envs + (curenv - envs + 1) % NENV;
	} else {
		env = envs;
	}
	struct Env *candidate_env = env;

	do {
		// Если нашли RUNNABLE, то запускаем, иначе идем дальше
		if (env && env->env_status == ENV_RUNNABLE) {
			// cprintf("Running env: %d\n", ENVX(env->env_id));
			env_run(env);
		}
		env = envs + (env - envs + 1) % NENV;
	} while (env != candidate_env);

	// К этому моменту мы посмотрели весь список и там никто не готов запуститься
	// Значит, если текущий CURENV запущен, то продолжаем его
	if (curenv && curenv->env_status == ENV_RUNNING) {
		// cprintf("Continue env: %d\n", ENVX(env->env_id));
		env_run(curenv);
	}
	
	// sched_halt never returns
	sched_halt();
}

// Halt this CPU when there is nothing to do. Wait until the
// timer interrupt wakes it up. This function never returns.
//
void
sched_halt(void)
{
	int i;

	// For debugging and testing purposes, if there are no runnable
	// environments in the system, then drop into the kernel monitor.
	for (i = 0; i < NENV; i++) {
		if ((envs[i].env_status == ENV_RUNNABLE ||
		     envs[i].env_status == ENV_RUNNING))
			break;
	}
	if (i == NENV) {
		cprintf("No runnable environments in the system!\n");
		while (1)
			monitor(NULL);
	}

	// Mark that no environment is running on CPU
	curenv = NULL;

	// Reset stack pointer, enable interrupts and then halt.
	asm volatile (
		"movl $0, %%ebp\n"
		"movl %0, %%esp\n"
		"pushl $0\n"
		"pushl $0\n"
		"sti\n"
		"hlt\n"
	: : "a" (cpu_ts.ts_esp0));
}

