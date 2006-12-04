;; offsets en la GDT de las syscalls
	 
%define CREATE_TASK		(0x4 << 3)
%define DESTROY_TASK		(0x5 << 3)
%define GET_CURRENT_TASK	(0x6 << 3)
		
%define CREATE_THREAD		(0x7 << 3)
%define DESTROY_THREAD		(0x8 << 3)
%define SET_THREAD_RUN_PERM	(0x9 << 3)
%define SET_THREAD_RUN_MODE	(0xa << 3)
%define RUN_THREAD		(0xb << 3)
%define GET_CURRENT_THREAD	(0xc << 3)

%define PAGE_IN			(0xd << 3)
%define PAGE_OUT		(0xe << 3)
%define FLUSH_TLB		(0xf << 3)    
%define GET_PAGE_FAULT          (0x10 << 3)
    
%define CREATE_INT_HANDLER	(0x11 << 3)
%define DESTROY_INT_HANDLER	(0x12 << 3)
%define RET_FROM_INT		(0x13 << 3)
%define GET_LAST_INT		(0x14 << 3)

%define OPEN_PORT		(0x15 << 3)
%define CLOSE_PORT		(0x16 << 3)
%define SET_PORT_PERM		(0x17 << 3)
%define SET_PORT_MODE		(0x18 << 3)	
%define SEND_MSG		(0x19 << 3)
%define GET_MSG			(0x1a << 3)
%define GET_MSG_COUNT		(0x1b << 3)
	
%define SHARE_MEM		(0x1c << 3)
%define CLAIM_MEM		(0x1d << 3)
%define READ_MEM		(0x1e << 3)
%define WRITE_MEM		(0x1f << 3)
%define PASS_MEM		(0x20 << 3)
%define MEM_SIZE                (0x21 << 3)