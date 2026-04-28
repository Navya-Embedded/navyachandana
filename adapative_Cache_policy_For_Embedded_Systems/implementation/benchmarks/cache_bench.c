#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Loop: tight repeated access over small array — LRU-friendly */
void bench_loop(volatile int*a,int n,int iters){
    for(int i=0;i<iters;i++)for(int j=0;j<n;j++)a[j]+=a[(j+1)%n];
    printf("loop a[0]=%d\n",a[0]);}
/* Stream: single linear scan over large array — FIFO-friendly */
void bench_stream(volatile int*a,int n){
    long s=0;for(int i=0;i<n;i++)s+=a[i];printf("stream sum=%ld\n",s);}
/* Mixed: alternating stream + loop phases — Adaptive wins */
void bench_mixed(volatile int*big,int bn,volatile int*small,int sn,int phases){
    for(int p=0;p<phases;p++){
        long s=0;for(int i=0;i<bn;i++)s+=big[i];
        for(int r=0;r<50;r++)for(int j=0;j<sn;j++)small[j]^=small[(j+3)%sn];}
    printf("mixed small[0]=%d\n",small[0]);}
/* Stride: pathological for LRU due to recency inversion */
void bench_stride(volatile int*a,int n,int stride){
    long s=0;for(int r=0;r<30;r++)for(int i=0;i<n;i+=stride){s+=a[i];a[i]++;}
    printf("stride sum=%ld\n",s);}
int main(int argc,char*argv[]){
    const int BIG=1<<22,SMALL=1<<12,MED=1<<16;
    int*mem=(int*)malloc(BIG*sizeof(int));
    if(!mem){perror("malloc");return 1;}
    for(int i=0;i<BIG;i++)mem[i]=i&0xffff;
    const char*mode=argc>1?argv[1]:"mixed";
    if     (!strcmp(mode,"loop"))  bench_loop(mem,SMALL,3000);
    else if(!strcmp(mode,"stream"))bench_stream(mem,BIG);
    else if(!strcmp(mode,"mixed")) bench_mixed(mem,MED,mem+MED,SMALL,8);
    else if(!strcmp(mode,"stride"))bench_stride(mem,MED,64);
    else{fprintf(stderr,"unknown mode\n");return 1;}
    free(mem);return 0;}
