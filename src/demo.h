#ifndef DEMO_H_
#define DEMO_H_

long demo_time_msec;

int demo_init(void);
void demo_cleanup(void);

void demo_display(void);
void demo_reshape(int x, int y);
void demo_keyboard(int key, int pressed);
void demo_mouse(int bn, int pressed, int x, int y);
void demo_motion(int x, int y);

#endif	/* DEMO_H_ */
