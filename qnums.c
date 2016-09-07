#define cmpfn(i,j) ((i)-(j))
/*****************************************************************************
 * Quick Sort routine for numbers
 *****************************************************************************
 */
static __inline int * find_any(low, high, match_word)
long int * low;
long int * high;
long int  match_word;
{
long int* guess;
long int i;

    while (low <= high)
    {
        guess = low + ((high - low) >> 1);
        i = cmpfn(*guess, match_word);
        if (i == 0)
            return guess + 1; 
        else
        if (i < 0)
            low = guess + 1;
        else
            high = guess - 1;
    }
    return low;
}
static void qnums(a1, cnt)
long int *a1;                /* Array of numbers or pointers to be sorted */
int cnt;                     /* Number of numbers to be sorted            */
{
long int mid;
long int swp;
long int*top; 
long int*bot; 
long int*cur; 
long int*ptr_stack[128];
long int cnt_stack[128];
long int stack_level = 0;
long int i, j, l;

    ptr_stack[0] = a1;
    cnt_stack[0] = cnt;
    stack_level = 1;
/*
 * Now Quick Sort the stuff
 */
    while (stack_level > 0)
    {
        stack_level--;
        cnt = cnt_stack[stack_level];
        a1 = ptr_stack[stack_level];
        top = a1 + cnt - 1;
        if (cnt < 4)
        {
            for (bot = a1; bot < top; bot++)
                 for (cur = top ; cur > bot; cur--)
                 {
                     if (cmpfn(*bot, *cur) > 0)
                     {
                         mid = *bot;
                         *bot = *cur;
                         *cur = mid;
                     }
                 }
            continue;
        }
        bot = a1;
        cur = a1 + 1;
/*
 * Check for all in order
 */
        while (bot < top && cmpfn(*bot, *cur) <= 0)
        {
            bot++;
            cur++;
        }
        if (cur > top)
            continue;
                       /* They are all the same, or they are in order */
        mid = *cur;
        if (cur == top)
        {              /* One needs to be swapped, then we are done? */
            *cur = *bot;
            *bot = mid;
            cur = bot - 1;
            top = find_any(a1, cur, mid);
            if (top == bot)
                continue;
            while (cur >= top)
                *bot-- = *cur--;
            *bot = mid;
            continue;
        }
        if (cnt > 18)
        {
        long int samples[31];  /* Where we are going to put samples */

            samples[0] = mid; /* Ensure that we do not pick the highest value */
            l = bot - a1;
            if (cnt < (2 << 5))
                i = 3;
            else 
            if (cnt < (2 << 7))
                i = 5;
            else 
            if (cnt < (2 << 9))
                i = 7;
            else 
            if (cnt < (2 << 11))
                i = 11;
            else 
            if (cnt < (2 << 15))
                i = 19;
            else 
                i = 31;
            if (l >= i && cmpfn(a1[l >> 1], a1[l]) < 0)
            {
                mid = a1[l >> 1];
                bot = a1;
            }
            else
            {
                j = cnt/i;
                for (bot = &samples[i - 1]; bot > &samples[0]; top -= j)
                    *bot-- = *top;
                qnums(&samples[0], i);
                cur = &samples[(i >> 1)];
                top = &samples[i - 1];
                while (cur > &samples[0] && !cmpfn(*cur, *top))
                    cur--;
                mid = *cur;
                top = a1 + cnt - 1;
                if (cmpfn(*(a1 + l), mid) <= 0)
                    bot = a1 + l + 2;
                else
                    bot = a1;
            }
        }
        else
            bot = a1;
        while (bot < top)
        {
            while (bot < top && cmpfn(*bot, mid) < 1)
                bot++;
            if (bot >= top)
                break;
            while (bot < top && cmpfn(*top, mid) > 0)
                top--;
            if (bot >= top)
                break;
            swp = *bot;
            *bot++ = *top;
            *top-- = swp;
        }
        if (bot == top && cmpfn(*bot, mid) < 1)
            bot++;
        if (stack_level > 125)
        {
            qnums(a1, bot - a1);
            qnums(bot, cnt - (bot - a1));
        }
        else
        {
            ptr_stack[stack_level] = a1;
            cnt_stack[stack_level] = bot - a1;
            stack_level++;
            ptr_stack[stack_level] = bot;
            cnt_stack[stack_level] = cnt - (bot - a1);
            stack_level++;
        }
    }
    return;
}
