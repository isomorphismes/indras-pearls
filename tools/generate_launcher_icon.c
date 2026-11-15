/*
 * Generate the {7,3} Poincare disk launcher icon.
 * Build: cc -std=c11 -O2 -Iandroid/app/src/main/cpp \
 *   tools/generate_launcher_icon.c android/app/src/main/cpp/mobius_math.c \
 *   -lm -o /tmp/indras-icon
 * Run: /tmp/indras-icon > android/app/src/main/res/drawable/ic_indras_pearls.xml
 *
 * Uses the application's complex arithmetic; no Java, Kotlin or DEX.
 * The reflections are inversions in circles orthogonal to the unit disk.
 */
#include "mobius_math.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define SIDES 7
#define MAX_TILES 512
#define DEPTH 3
#define PI 3.14159265358979323846264338327950288

typedef struct complex_number Complex;
typedef struct { Complex vertex[SIDES]; int depth; } Tile;
static Tile tiles[MAX_TILES];
static int count;

static Complex z(double x, double y) { return complex_make(x,y); }
static Complex sub(Complex a, Complex b) { return complex_subtract(a,b); }
static Complex add(Complex a, Complex b) { return complex_add(a,b); }
static Complex mul(Complex a, Complex b) { return complex_multiply(a,b); }
static Complex scale(Complex a,double s) { return mul(a,z(s,0)); }
static double norm2(Complex a) { return a.real*a.real+a.imaginary*a.imaginary; }
static Complex conjugate(Complex a) { return z(a.real,-a.imaginary); }

/* Euclidean center of the unique circle through a,b orthogonal to |z|=1. */
static bool geodesic(Complex a, Complex b, Complex *center, double *radius) {
    double aa=1+norm2(a), bb=1+norm2(b);
    double determinant=2*(a.real*b.imaginary-a.imaginary*b.real);
    if (fabs(determinant)<1e-11) return false;
    center->real=(aa*b.imaginary-bb*a.imaginary)/determinant;
    center->imaginary=(a.real*bb-b.real*aa)/determinant;
    double rr=norm2(*center)-1;
    if (rr<=0) return false;
    *radius=sqrt(rr);
    return true;
}
static Complex reflect(Complex point, Complex center, double radius) {
    Complex delta=sub(point,center);
    return add(center,scale(delta,(radius*radius)/norm2(delta)));
}
static Complex centroid(const Tile *tile) {
    Complex sum=z(0,0);
    for(int i=0;i<SIDES;i++) sum=add(sum,tile->vertex[i]);
    return scale(sum,1.0/SIDES);
}
static bool already_present(const Tile *candidate) {
    Complex center=centroid(candidate);
    for(int i=0;i<count;i++) {
        Complex diff=sub(center,centroid(&tiles[i]));
        if(norm2(diff)<1e-9) return true;
    }
    return false;
}
static void emit_edge(Complex a, Complex b) {
    Complex center;
    double radius;
    if (!geodesic(a,b,&center,&radius)) {
        printf(" L %.4f,%.4f",54+38*b.real,54-38*b.imaginary);
        return;
    }
    double first=atan2(a.imaginary-center.imaginary,a.real-center.real);
    double last=atan2(b.imaginary-center.imaginary,b.real-center.real);
    double delta=remainder(last-first,2*PI);
    /* Choose the arc inside the Poincare disk. */
    Complex mid=add(center,z(radius*cos(first+delta/2),radius*sin(first+delta/2)));
    if(norm2(mid)>1.0) delta+=delta>0 ? -2*PI : 2*PI;
    for(int step=1;step<=12;step++) {
        double angle=first+delta*step/12.0;
        Complex point=add(center,z(radius*cos(angle),radius*sin(angle)));
        printf(" L %.4f,%.4f",54+38*point.real,54-38*point.imaginary);
    }
}
static void emit_tile(const Tile *tile) {
    Complex a=tile->vertex[0];
    printf("M %.4f,%.4f",54+38*a.real,54-38*a.imaginary);
    for(int i=0;i<SIDES;i++) emit_edge(tile->vertex[i],tile->vertex[(i+1)%SIDES]);
    printf(" Z ");
}
int main(void) {
    /* Hyperbolic edge length: cosh(edge/2)=cos(pi/q)/sin(pi/p). */
    double half_edge=acosh(cos(PI/3)/sin(PI/7));
    double sinh_radius=sinh(half_edge)/sin(PI/7);
    double disk_radius=sinh_radius/(sqrt(1+sinh_radius*sinh_radius)+1);
    Tile seed={0};
    for(int i=0;i<SIDES;i++) {
        double angle=2*PI*i/SIDES+PI/2;
        seed.vertex[i]=z(disk_radius*cos(angle),disk_radius*sin(angle));
    }
    tiles[count++]=seed;
    for(int index=0;index<count;index++) {
        Tile parent=tiles[index];
        if(parent.depth>=DEPTH) continue;
        for(int edge=0;edge<SIDES;edge++) {
            Complex center;
            double radius;
            if(!geodesic(parent.vertex[edge],parent.vertex[(edge+1)%SIDES],&center,&radius)) continue;
            Tile child={0};
            child.depth=parent.depth+1;
            bool valid=true;
            for(int k=0;k<SIDES;k++) {
                child.vertex[k]=reflect(parent.vertex[k],center,radius);
                if(!isfinite(child.vertex[k].real)||!isfinite(child.vertex[k].imaginary)||
                   norm2(child.vertex[k])>=1.0) valid=false;
            }
            if(valid && !already_present(&child) && count<MAX_TILES) tiles[count++]=child;
        }
    }
    puts("<?xml version=\"1.0\" encoding=\"utf-8\"?>");
    puts("<vector xmlns:android=\"http://schemas.android.com/apk/res/android\""
         " android:width=\"108dp\" android:height=\"108dp\""
         " android:viewportWidth=\"108\" android:viewportHeight=\"108\">");
    puts("<path android:fillColor=\"#22262B\" android:pathData=\"M0,0h108v108h-108z\"/>");
    printf("<path android:fillColor=\"#00000000\" android:strokeColor=\"#B9D9E5\""
           " android:strokeWidth=\"0.85\" android:pathData=\"");
    for(int i=0;i<count;i++) emit_tile(&tiles[i]);
    puts("\"/>");
    puts("<path android:fillColor=\"#00000000\" android:strokeColor=\"#E4EDF2\""
         " android:strokeWidth=\"1.5\""
         " android:pathData=\"M92,54 A38,38 0,1 1,16 54 A38,38 0,1 1,92 54\"/>");
    puts("</vector>");
    return 0;
}
