;; GDT syscall offsets
     
%define CREATE_TASK              (0x5 << 3)
%define INIT_TASK                (0x6 << 3)
%define DESTROY_TASK             (0x7 << 3)
%define GET_CURRENT_TASK         (0x8 << 3)
        
%define CREATE_THREAD            (0x9 << 3)
%define DESTROY_THREAD           (0xa << 3)
%define SET_THREAD_RUN_PERMS     (0xb << 3)
%define SET_THREAD_RUN_MODE      (0xc << 3)
%define RUN_THREAD               (0xd << 3)
%define RUN_THREAD_INT           (0xe << 3)
%define GET_CURRENT_THREAD       (0xf << 3)

%define PAGE_IN                  (0x10 << 3)
%define PAGE_OUT                 (0x11 << 3)
%define FLUSH_TLB                (0x12 << 3)    
%define GET_PAGE_FAULT           (0x13 << 3)
%define GRANT_PAGE_MK            (0x14 << 3)
    
%define CREATE_INT_HANDLER       (0x15 << 3)
%define DESTROY_INT_HANDLER      (0x16 << 3)
%define RET_FROM_INT             (0x17 << 3)
%define GET_LAST_INT             (0x18 << 3)
%define GET_LAST_INT_ADDR        (0x19 << 3)

%define OPEN_PORT                (0x1a << 3)
%define CLOSE_PORT               (0x1b << 3)
%define SET_PORT_PERM            (0x1c << 3)
%define SET_PORT_MODE            (0x1d << 3)    
%define SEND_MSG                 (0x1e << 3)
%define GET_MSG                  (0x1f << 3)
%define GET_MSG_COUNT            (0x20 << 3)
%define GET_MSGS                 (0x21 << 3)
%define GET_MSG_COUNTS           (0x22 << 3)
    
%define SHARE_MEM                (0x23 << 3)
%define CLAIM_MEM                (0x24 << 3)
%define READ_MEM                 (0x25 << 3)
%define WRITE_MEM                (0x26 << 3)
%define PASS_MEM                 (0x27 << 3)
%define MEM_SIZE                 (0x28 << 3)

%define POP_INT                  (0x29 << 3)
%define PUSH_INT                 (0x2a << 3)
%define RESUME_INT               (0x2b << 3)

%define LAST_ERROR               (0x2c << 3)

%define TTRACE_BEGIN             (0x2d << 3)
%define TTRACE_END               (0x2e << 3)
%define TTRACE_REG               (0x2f << 3)
%define TTRACE_MEM_READ          (0x30 << 3)
%define TTRACE_MEM_WRITE         (0x31 << 3)

%define EVT_SET_LISTENER         (0x32 << 3)
%define EVT_WAIT                 (0x33 << 3)
%define EVT_DISABLE              (0x34 << 3)

%define IDLE_CPU                 (0x35 << 3)

%ifdef _METRICS_
%define GET_METRICS              (0x36 << 3)
%endif

