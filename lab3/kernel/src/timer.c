#include "timer.h"
#include "uart1.h"
#include "heap.h"
#include "u_string.h"

#define STR(x) #x
#define XSTR(s) STR(s)

struct list_head* timer_event_list;  // first head has nothing, store timer_event_t after it 

void timer_list_init(){
    INIT_LIST_HEAD(timer_event_list);
}

void core_timer_enable(){
    __asm__ __volatile__(
        "mov x1, 1\n\t"
        "msr cntp_ctl_el0, x1\n\t" //cntp_ctl_el0: 1 enable EL1 physical timer.
        "mov x2, 2\n\t"
        "ldr x1, =" XSTR(CORE0_TIMER_IRQ_CTRL) "\n\t" //ldr x1, =CORE0_TIMER_IRQ_CTRL #define CORE0_TIMER_IRQ_CTRL 0x40000040
        "str w2, [x1]\n\t"         // unmask timer interrupt
                                   // QA7_rev3.4.pdf: Core0 Timer IRQ allows Non-secure physical timer(nCNTPNSIRQ)
    );
}

void core_timer_disable()
{
    __asm__ __volatile__(
        "mov x2, 0\n\t"
        "ldr x1, =" XSTR(CORE0_TIMER_IRQ_CTRL) "\n\t"
        "str w2, [x1]\n\t"        //mask timer interrupt
    );
}


// set timer interrupt time to [expired_time] seconds after now (relatively)
void set_core_timer_interrupt(unsigned long long etime){
    __asm__ __volatile__(
        "mrs x1, cntfrq_el0\n"    // cntfrq_el0: frequency of the timer
        "mul x1, x1, %0\n"        // cntpct_el0 = cntfrq_el0 * seconds: relative timer to cntfrq_el0
        "msr cntp_tval_el0, x1\n" // Set expired time to cntp_tval_el0, 會把cval設為now(cntpct)+tval 
    :"=r" (etime));
}

// set timer interrupt time to a cpu tick  (directly) 
void set_core_timer_interrupt_by_tick(unsigned long long tick){
    __asm__ __volatile__(
        "msr cntp_cval_el0, %0\n\t"  //cntp_cval_el0  (when cntpct >= cval, trigger timer interrupt)
    :"=r" (tick));
}

// calculate cpu tick after n second from now 
unsigned long long get_tick_plus_s(unsigned long long second){
    unsigned long long cntpct_el0=0;
    __asm__ __volatile__("mrs %0, cntpct_el0\n\t": "=r"(cntpct_el0)); // now
    unsigned long long cntfrq_el0=0;
    __asm__ __volatile__("mrs %0, cntfrq_el0\n\t": "=r"(cntfrq_el0)); // tick frequency
    return (cntpct_el0 + cntfrq_el0*second); 
}


void core_timer_handler(){ //when trigger timer exception
    //uart_sendline("===in core timer handler===\n");
    if (list_empty(timer_event_list))
    {
        set_core_timer_interrupt(10000); // disable timer interrupt (set a very big value)
        return;
    }
    timer_event_callback((timer_event_t *)timer_event_list->next); // do callback and set new interrupt
    //uart_sendline("===end core timer handler===\n");
}


void timer_event_callback(timer_event_t * timer_event){
    list_del_entry((struct list_head*)timer_event); // delete the event in queue

    ((void (*)(char*))timer_event-> callback)(timer_event->args);  // call the event

    // set queue linked list to next time event if it exists
    if(!list_empty(timer_event_list))
    {
        set_core_timer_interrupt_by_tick(((timer_event_t*)timer_event_list->next)->interrupt_time);
    }
    else
    {
        set_core_timer_interrupt(10000);  // disable timer interrupt (set a very big value)
    }
}


//implement basic 2
void timer_2s(char* str)
{
    unsigned long long cntpct_el0;
    __asm__ __volatile__("mrs %0, cntpct_el0\n\t": "=r"(cntpct_el0)); // tick auchor
    unsigned long long cntfrq_el0;
    __asm__ __volatile__("mrs %0, cntfrq_el0\n\t": "=r"(cntfrq_el0)); // tick frequency
    uart_sendline("[Interrupt][el1_irq][%s] %d seconds after booting\n", str, cntpct_el0/cntfrq_el0);
    add_timer(timer_2s,2,"timer2s");
}


void add_timer(void *callback, unsigned long long timeout, char* args){
    timer_event_t* the_timer_event = kmalloc(sizeof(timer_event_t)); // free by timer_event_callback
    //information in timer_event
    the_timer_event->args = kmalloc(strlen(args)+1);
    strcpy(the_timer_event -> args,args);
    the_timer_event->interrupt_time = get_tick_plus_s(timeout);
    the_timer_event->callback = callback;
    INIT_LIST_HEAD(&the_timer_event->listhead);

    // add the timer_event into timer_event_list (sorted)
    struct list_head* curr;
    list_for_each(curr,timer_event_list) //curr list & global timer_event_list 
    {
        if(((timer_event_t*)curr)->interrupt_time > the_timer_event->interrupt_time)
        {
            list_add(&the_timer_event->listhead,curr->prev);  // add this timer "before" the bigger one
            break;
        }
    }
    // if the timer_event is the biggest, add at tail
    if(list_is_head(curr,timer_event_list))
    {
        list_add_tail(&the_timer_event->listhead,timer_event_list);
    }
    // set interrupt to first event
    set_core_timer_interrupt_by_tick(((timer_event_t*)timer_event_list->next)->interrupt_time);
}

void print_time(){
    unsigned long long cntpct_el0;
    __asm__ __volatile__("mrs %0, cntpct_el0\n":"=r"(cntpct_el0)); //get cntpct_el0
    unsigned long long cntfrq_el0;
    __asm__ __volatile__("mrs %0, cntfrq_el0\n":"=r"(cntfrq_el0)); //get cntfrq_el0
    uart_sendline("set command time: %d \n",cntpct_el0/cntfrq_el0);
}
