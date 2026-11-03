#include "pid.h"

void  pid_init(pid_t *p, float kp, float ki, float kd, float out_min, float out_max)
{
    p->kp = kp;
    p->ki = ki;
    p->kd = kd;

    p->out_min = out_min;
    p->out_max = out_max;

    p->iout_min = -0.2f * out_max;
    p->iout_max =  0.2f * out_max;


    p->set      = 0.0f;
    p->fdb      = 0.0f;
    p->error[0] = 0.0f;
    p->error[1] = 0.0f;
    p->pout     = 0.0f;
    p->iout     = 0.0f;
    p->dout     = 0.0f;
    p->out      = 0.0f;
    p->ready    = 0;
}

void  pid_set_integral_limit(pid_t *p, float iout_min, float iout_max)//存限幅，不同限幅区别
{
    p->iout_min = iout_min;
    p->iout_max = iout_max;
}

float pid_calc(pid_t *p, float target, float meas)
{
    p->set = target;
    p->fdb = meas;

    p->error[1] = p->error[0];
    p->error[0] = target - meas;

    p->pout = p->kp * p->error[0];

    p->iout += p->ki * p->error[0];
      
    if (p->iout < p->iout_min) p->iout = p->iout_min;
    if (p->iout > p->iout_max) p->iout = p->iout_max;

    /* 首拍避免 D 项冲击 */
    float diff = p->error[0] - p->error[1];

    if (p->ready == 0) 
    {
        diff   = 0.0f;
        p->ready = 1;
    }

    p->dout = p->kd * diff; 

    p->out = p->pout + p->iout + p->dout;
    if (p->out > p->out_max)      p->out = p->out_max;
    else if (p->out < p->out_min) p->out = p->out_min;
    return p->out;
}

void pid_reset(pid_t *p)
{
    p->iout     = 0.0f;
    p->error[0] = 0.0f;
    p->error[1] = 0.0f;
    p->pout     = 0.0f;
    p->dout     = 0.0f;
    p->out      = 0.0f;
    
    p->set = 0.0f;
    p->fdb = 0.0f;

    p->ready    = 0;
}