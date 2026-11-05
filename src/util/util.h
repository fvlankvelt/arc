#ifndef __UTIL_H__
#define __UTIL_H__

#define for_each_or_once(cond, arr, item, len)                      \
    typeof(*arr) item = arr[0];                                     \
    for (int i_##item = 0; !i_##item || ((cond) && i_##item < len); \
         i_##item++, item = arr[i_##item + 1])

#endif // __UTIL_H__