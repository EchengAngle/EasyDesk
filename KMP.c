
#include <stdio.h>
#include <string.h>
#include <iostream>

// make use of the KMP algorithm to find the macthing pattern's 
// postion in a specific text string.

int getKMPnext(const char* pattern, int* nextval)
{

    int len = strlen(pattern);

    int i=1; // postfix 
    int j= 0; // prefix

    nextval[0] = 0;

    while(i < len)
    {
        if(pattern[i] == pattern[j])
        {
            nextval[i] = j+1;
            i++;
            j++;
        }
        else
        {
            if(j > 0)
                j = nextval[j-1];
            else
            {
                nextval[j] = 0;
                j=0;
                i++;
            }
        }
    }

    return 0;
}

// find the index of the pattren in string.
int getStrIndex(const char * str, const char* pattern)
{

    int sLen = strlen(str);
    int pLen = strlen(pattern);
    if(pLen == 0 ) return 0; // the pattern is empty;

    int next[pLen];
    int ret = getKMPnext(pattern, next);
    int i =0, j= 0;
    while(i < sLen)
    {
        if(str[i] == pattern[j])
        {
            ++i;
            ++j;
        }
        else{
            if(j > 0)
            {
                j = next[j-1];
            }
            else{
                ++i;
            }
        }
        if (j == pLen)
        {
            return i-j;
        }
    }
    return -1; // can't find the matching pattern.
    
}

int main()
{

    const char * text = "aadbdcsabababcddx";
    const char * pattern = "ababc"; //next value: 00120

    int ret = getStrIndex(text, pattern);
   
    std::cout << ret << ", " << text + ret << std::endl;

    return 0;
}
