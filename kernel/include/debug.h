#ifndef __DEBUG_H__
#define __DEBUG_H__

extern volatile unsigned char _flag;

void debug_init(void);
void test_excp_de(void);
void test_excp_pf(void);
void kern_abort(const char *file, int line, const char *func, char *msg);

#define KERN_ABORT(msg) kern_abort(__FILE__, __LINE__, __FUNCTION__, msg)


#endif /* __DEBUG_H__ */
