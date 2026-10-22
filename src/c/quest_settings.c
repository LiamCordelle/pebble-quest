#include "lab.h"
#include "quest_settings.h"
#include <string.h>
#ifdef LAB_HOST_RENDER
#include <stdlib.h>
#else
#include "kit/kit.h"
#endif
static char s_name[9]="LIAM";
void quest_normalize_name(char out[9],const char *in) {
  unsigned n=0;
  for(;in && *in && n<8;in++) {
    unsigned char c=*in;
    if(c>='a' && c<='z')c-=32;
    if((c>='A' && c<='Z') || (c>='0' && c<='9') || c==' ')out[n++]=c;
  }
  while(n && out[n-1]==' ')n--;
  out[n]=0;
  unsigned start=0;while(start<n && out[start]==' ')start++;
  if(start)memmove(out,out+start,n-start+1);
  if(!out[0])strcpy(out,"LIAM");
}
#ifndef LAB_HOST_RENDER
#define HERO_KEY 2000
bool kit_consumer_settings_receive(DictionaryIterator *iter) {
  Tuple *t=dict_find(iter,MESSAGE_KEY_HERO_NAME);
  if(!t || t->type!=TUPLE_CSTRING || t->length<1 || t->length>33)return false;
  char incoming[33];memcpy(incoming,t->value->cstring,t->length);
  if(incoming[t->length-1]!=0)return false;
  char next[9];quest_normalize_name(next,incoming);
  if(!strcmp(next,s_name))return false;
  strcpy(s_name,next);
  if(persist_write_string(HERO_KEY,s_name)<0)APP_LOG(APP_LOG_LEVEL_WARNING,"Hero name save failed");
  APP_LOG(APP_LOG_LEVEL_INFO,"Hero name applied: %s",s_name);
  return true;
}
#endif
const char *quest_hero_name(void) {
#ifdef LAB_HOST_RENDER
  const char *name=getenv("QUEST_HERO_NAME");if(name)quest_normalize_name(s_name,name);
#endif
  return s_name;
}
#ifndef LAB_HOST_RENDER
void kit_consumer_settings_init(void) {
  char saved[33];
  if(persist_read_string(HERO_KEY,saved,sizeof(saved))>0)quest_normalize_name(s_name,saved);
}
#endif
