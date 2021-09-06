#include <linux/clocksource.h>
#include <linux/clockchips.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <asm/host_ops.h>

static unsigned long long boot_time;

void __ndelay_fuzz(unsigned long nsecs)
{
	unsigned long long start;

	if(lkl_ops->fuzz_ops->is_active() && lkl_ops->fuzz_ops->minimize_delays()) {
		nsecs = lkl_ops->fuzz_ops->minimize_delays();
	}
	start = lkl_ops->time();
	while (lkl_ops->time() < start + nsecs)
		;
}


void __ndelay(unsigned long nsecs)
{
	unsigned long long start;
	start = lkl_ops->time();
	while (lkl_ops->time() < start + nsecs)
		;
}

void __udelay(unsigned long usecs)
{
	__ndelay(usecs * NSEC_PER_USEC);
}
EXPORT_SYMBOL(__udelay);
void __udelay_fuzz(unsigned long usecs)
{
	__ndelay_fuzz(usecs * NSEC_PER_USEC);
}
EXPORT_SYMBOL(__udelay_fuzz);

void __const_udelay(unsigned long xloops)
{
	__udelay(xloops / 0x10c7ul);
}
EXPORT_SYMBOL(__const_udelay);
void __const_udelay_fuzz(unsigned long xloops)
{
	__udelay_fuzz(xloops / 0x10c7ul);
}
EXPORT_SYMBOL(__const_udelay_fuzz);

void __delay(unsigned long xloops)
{
	__udelay(xloops / 0x10c7ul);
}
EXPORT_SYMBOL(__delay);
void __delay_fuzz(unsigned long xloops)
{
	__udelay_fuzz(xloops / 0x10c7ul);
}
EXPORT_SYMBOL(__delay_fuzz);

#define orig_time_after(a,b)		\
	(typecheck(unsigned long, a) && \
	 typecheck(unsigned long, b) && \
	 ((long)((b) - (a)) < 0))
#define orig_time_before(a,b)	orig_time_after(b,a)

#define orig_time_after_eq(a,b)	\
	(typecheck(unsigned long, a) && \
	 typecheck(unsigned long, b) && \
	 ((long)((a) - (b)) >= 0))
#define orig_time_before_eq(a,b)	orig_time_after_eq(b,a)
int time_after_fuzz(unsigned long a, unsigned long b) {
	if(lkl_ops->fuzz_ops->is_active() && lkl_ops->fuzz_ops->minimize_timeafter()) {
		b = a + lkl_ops->fuzz_ops->minimize_timeafter();
	}
	return orig_time_after(a, b);
}
EXPORT_SYMBOL(time_after_fuzz);
int time_before_fuzz(unsigned long a, unsigned long b) {
	if(lkl_ops->fuzz_ops->is_active() && lkl_ops->fuzz_ops->minimize_timebefore()) {
		b = a - 1;
	}
	return orig_time_before(a, b);
}
EXPORT_SYMBOL(time_before_fuzz);
int time_after_eq_fuzz(unsigned long a, unsigned long b) {
	if(lkl_ops->fuzz_ops->is_active() && lkl_ops->fuzz_ops->minimize_timeafter()) {
		b = a + 1;
	}
	return orig_time_after_eq(a, b);
}
EXPORT_SYMBOL(time_after_eq_fuzz);
int time_before_eq_fuzz(unsigned long a, unsigned long b) {
	if(lkl_ops->fuzz_ops->is_active() && lkl_ops->fuzz_ops->minimize_timebefore()) {
		b = a - 1;
	}
	return orig_time_before_eq(a, b);
}
EXPORT_SYMBOL(time_before_eq_fuzz);

#define orig_time_after64(a,b)	\
	(typecheck(__u64, a) &&	\
	 typecheck(__u64, b) && \
	 ((__s64)((b) - (a)) < 0))
#define orig_time_before64(a,b)	orig_time_after64(b,a)

#define orig_time_after_eq64(a,b)	\
	(typecheck(__u64, a) && \
	 typecheck(__u64, b) && \
	 ((__s64)((a) - (b)) >= 0))
#define orig_time_before_eq64(a,b)	orig_time_after_eq64(b,a)
int time_after64_fuzz(__u64 a, __u64 b) {
	if(lkl_ops->fuzz_ops->is_active() && lkl_ops->fuzz_ops->minimize_timeafter()) {
		b = a + 1;
	}
	return orig_time_after64(a, b);
}
EXPORT_SYMBOL(time_after64_fuzz);
int time_before64_fuzz(__u64 a, __u64 b) {
	if(lkl_ops->fuzz_ops->is_active() && lkl_ops->fuzz_ops->minimize_timebefore()) {
		b = a - 1;
	}
	return orig_time_before64(a, b);
}
EXPORT_SYMBOL(time_before64_fuzz);
int time_after_eq64_fuzz(__u64 a, __u64 b) {
	if(lkl_ops->fuzz_ops->is_active() && lkl_ops->fuzz_ops->minimize_timeafter()) {
		b = a + 1;
	}
	return orig_time_after_eq64(a, b);
}
EXPORT_SYMBOL(time_after_eq64_fuzz);
int time_before_eq64_fuzz(__u64 a, __u64 b) {
	if(lkl_ops->fuzz_ops->is_active() && lkl_ops->fuzz_ops->minimize_timebefore()) {
		b = a - 1;
	}
	return orig_time_before_eq64(a, b);
}
EXPORT_SYMBOL(time_before_eq64_fuzz);

void calibrate_delay(void)
{
}

// Note(feli): this causes compilation errors,
// but i think we don't need it
#if 0
void read_persistent_clock(struct timespec *ts)
{
	*ts = ns_to_timespec(lkl_ops->time());
}
#endif

/*
 * Scheduler clock - returns current time in nanosec units.
 *
 */
unsigned long long sched_clock(void)
{
	if (!boot_time)
		return 0;

	return lkl_ops->time() - boot_time;
}

static u64 clock_read(struct clocksource *cs)
{
	return lkl_ops->time();
}

static struct clocksource clocksource = {
	.name	= "lkl",
	.rating = 499,
	.read	= clock_read,
	.flags	= CLOCK_SOURCE_IS_CONTINUOUS,
	.mask	= CLOCKSOURCE_MASK(64),
};

static void *timer;

static int timer_irq;

static void timer_fn(void *arg)
{
	lkl_trigger_irq(timer_irq);
}

static int clockevent_set_state_shutdown(struct clock_event_device *evt)
{
	if (timer) {
		lkl_ops->timer_free(timer);
		timer = NULL;
	}

	return 0;
}

static int clockevent_set_state_oneshot(struct clock_event_device *evt)
{
	timer = lkl_ops->timer_alloc(timer_fn, NULL);
	if (!timer)
		return -ENOMEM;

	return 0;
}

static irqreturn_t timer_irq_handler(int irq, void *dev_id)
{
	struct clock_event_device *dev = (struct clock_event_device *)dev_id;

	dev->event_handler(dev);

	return IRQ_HANDLED;
}

static int clockevent_next_event(unsigned long ns,
				 struct clock_event_device *evt)
{
	return lkl_ops->timer_set_oneshot(timer, ns);
}

static struct clock_event_device clockevent = {
	.name			= "lkl",
	.features		= CLOCK_EVT_FEAT_ONESHOT,
	.set_state_oneshot	= clockevent_set_state_oneshot,
	.set_next_event		= clockevent_next_event,
	.set_state_shutdown	= clockevent_set_state_shutdown,
};

static struct irqaction irq0  = {
	.handler = timer_irq_handler,
	.flags = IRQF_NOBALANCING | IRQF_TIMER,
	.dev_id = &clockevent,
	.name = "timer"
};

void __init time_init(void)
{
	int ret;

	if (!lkl_ops->timer_alloc || !lkl_ops->timer_free ||
	    !lkl_ops->timer_set_oneshot || !lkl_ops->time) {
		pr_err("lkl: no time or timer support provided by host\n");
		return;
	}

	timer_irq = lkl_get_free_irq("timer");
	if(request_irq(timer_irq, irq0.handler, irq0.flags, irq0.name, irq0.dev_id) != 0) {
		pr_err("lkl: could setup timer irq %d\n", timer_irq);
		BUG();
	}


	ret = clocksource_register_khz(&clocksource, 1000000);
	if (ret)
		pr_err("lkl: unable to register clocksource\n");

	clockevents_config_and_register(&clockevent, NSEC_PER_SEC, 1, ULONG_MAX);

	boot_time = lkl_ops->time();
	pr_info("lkl: time and timers initialized (irq%d)\n", timer_irq);
}
