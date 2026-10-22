#include <assert.h>
#include <string.h>
#include "../src/c/quest_settings.h"
int main(void){char n[9];
quest_normalize_name(n,"abcdefghij");assert(!strcmp(n,"ABCDEFGH"));
quest_normalize_name(n," ! ");assert(!strcmp(n,"LIAM"));
quest_normalize_name(n,"a-b_9");assert(!strcmp(n,"AB9"));
quest_normalize_name(n,"  a b  ");assert(!strcmp(n,"A B"));
quest_normalize_name(n,NULL);assert(!strcmp(n,"LIAM"));return 0;}
