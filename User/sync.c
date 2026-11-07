#include "sync.h"

void sync_correct(float tl, float tr, float ml, float mr,  float *out_l, float *out_r)
{
    float e1  = tl - ml;         /* 左轮跟踪误差 */
    float e2  = tr - mr;         /* 右轮跟踪误差 */
    float eps = e1 - e2;         /* 同步误差 */

    float c = 0.5f * eps * SYNC_KC ;   /* 两边各让一半 */

    if (c >  SYNC_MAX_CORR) c =  SYNC_MAX_CORR;
    if (c < -SYNC_MAX_CORR) c = -SYNC_MAX_CORR;//限幅
    
    *out_l= tl+ c;
    *out_r= tr- c; 
}