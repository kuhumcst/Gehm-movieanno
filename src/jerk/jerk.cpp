/* JERK
Analyses the movement of two points in x-y plane, in casu nose tips' and other
body points' data from OpenPoseDemo.exe, and computes velocity, acceleration
and jerk of the points.

The program expects input with data for persons in a dialogue situation.

The format of the input is as follows:

FrameNr <tab> <first body part data> [<tab> <second body part data> ...] <newline>
  ...

where data for each body part is

<x-position> <tab> <y-position> <tab> <probability>

The probability value is currently ignored.

The command line arguments to the program are
  input file
  output file
  number of observations used for computing velocity
  number of observations used for computing acceleration
  number of observations used for computing jerk
  location of the observations with respect to the created annotation:
       'past' or 'middle'
  "name" of the first body point
 ["name" of the second body point
  "name" of the third body point
  ...]

For example

  jerk F2_M4_SPLIT_FINAL.tab F2_M4_SPLIT_FINAL-7-14-21-past.vaj.tab 7 14 21 past F2:Nose M4:Nose F2:Neck M4:Neck F2:MidHip M4:MidHip

Input is a tab separated text file. The number of columns is not completely fixed.
The first column is the frame number.
The second, third and fourth column are the x, y and reliability data of the first bodypoint to analyse.
If there are data for more than one body points in the same frame, add three columns for each body point,
so number of columns = 1 + 3 * number of body points.

Output format is
<frame number x 4> <tab> <bodyPoint A data> [<tab> <bodyPoint B data> ...] <new line>
  ...

<bodyPoint data> =
<x> <tab> <y> <tab> <probability> <tab> <velocity> <tab> <acceleration> <tab> <jerk> <new line>

where
  <velocity> = <strength> <tab> <angle (1..12)> <tab> <x component> <tab> <y component>
   etc.

<strength> and <angle> are just another (polar) representation of the components.
1..12 are 'clock' directions. '12' is up, '3' to the right, etc.

Author: Bart Jongejan
2020.01.23, 2021.06.14
*/

/*  */

#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#define M_PI           3.14159265358979323846  /* pi */

enum
    {
    evelo, eacce, ejerk
    };

struct bodyPoint
    {
    double x;
    double y;
    double p;
    double vx;
    double ax;
    double jx;
    double vy;
    double ay;
    double jy;
    };

int bodyPoints;

struct line
    {
    int frame;
    struct bodyPoint* P;
    line() :frame(-1), P(0)
        {
        P = new bodyPoint[bodyPoints];
        }
    };

int lines;

double velocity(double St, double St2, double Sh, double Sth, double period)
    {
    /*
    first degree: a:position, b:velocity
    var a
    solution (a.period^-1*(Sh+-1*St*b))
    var b
      solution
      (b.(-1*St^2+St2*period)^-1*(-1*Sh*St+Sth*period))

    For vertical, replace h th by v tv
    */
    return (Sth * period - Sh * St) / (St2 * period - St * St);
    }

double acceleration(double St, double St2, double St3, double St4, double Sh, double Sth, double St2h, double period)
    {

    /*

    acceleration c

    (horizontal)

    accumulate (S) t t2 t3 t4 h th t2h
    (vertical                 v tv t2v )

    solution
    ( c
    .     ( -1*St2^3
        + 2*St*St2*St3
        + St2*St4*period
        + -1*St^2*St4
        + -1*St3^2*period
        )
      ^ -1
    * ( -1*Sh*St2^2
      + Sh*St*St3
      + St*St2*Sth
      + St2*St2h*period
      + -1*St3*Sth*period
      + -1*St^2*St2h
      )
    )

    For vertical, replace h th t2h by v tv t2v
    */
    return
        (Sh * (St * St3 - St2 * St2)
         + Sth * (St * St2 - St3 * period)
         + St2h * (St2 * period - St * St)
         )
        /
        (St2 * (2 * St * St3 - St2 * St2 + St4 * period)
         - St * St * St4
         - St3 * St3 * period
         );

    }

double jerk(double St2, double St2h, double St3, double St3h, double St4, double St5, double St6, double Sth, double period)
    {
    double var1 = St3 * St3;
    double var2 = St4 * St4;
    double var5 = St3 * St5;
    double var6 = var1 - St2 * St4;

    return -(St2h * (St2 * St5 * period - St3 * (St2 * St2 + St4 * period)) + St3h * (St2 * St2 * St2 + period * var6) + Sth * (St2 * var6 + period * (var2 - var5)))
        / (var1 * var1 - St2 * (St5 * St5 * period - St2 * (var2 + 2 * var5 - St2 * St6) + St4 * (3 * var1 - St6 * period)) - period * (St6 * var1 + St4 * (var2 + -2.0 * var5)));
    }


void doBodyPoint(int BodyPointNr, int what, const char* Period, const char* margin, line* Lines)
    {
    size_t seqsiz = strtoul(Period, 0, 10); // number of observations used
    double maxSt2 = 0.0;
    int limit2 = (int)(seqsiz / 2 + 1);
    int limit1 = (int)(seqsiz / 4);
    int i;
    for(i = 1; i <= limit1; ++i)
        maxSt2 += (double)(2 * i * i);
    for(i = limit1 + 1; i <= limit2; ++i)
        maxSt2 += (double)(2 * i * i);
    double period = (double)seqsiz;         // Only used in divisions to get double results.
    long t0 = 0;
    long* Ts = (long*)calloc(seqsiz, sizeof(long));         // time stamps
    double* ts = (double*)calloc(seqsiz, sizeof(double));   // adjusted time stamps
    double* hs = (double*)calloc(seqsiz, sizeof(double));   // horizontal positions
    double* vs = (double*)calloc(seqsiz, sizeof(double));   // vertical positions
    size_t index = 0;               // Only increased when a valid point is found.
    struct line* current = Lines;   // current line

    size_t Offset;                  // offset in frames
    if(!strcmp(margin, "middle"))
        Offset = (seqsiz - 1) >> 1; // integer division by 2
    else
        Offset = 0;
    int ind = 0;
    for(current = Lines; ++ind < lines && current->frame >= 0; ++current)
        {
        struct bodyPoint* point = current->P + BodyPointNr; // current body point
        struct line* assignTo = current - Offset;           // line to assign the computed value to
        if(assignTo < Lines)    // cannot assign yet
            {
            assignTo = 0;
            }
        else
            {
            struct bodyPoint* assignToPoint = assignTo->P + BodyPointNr; // body point to assign the computed value to

            switch(what)
                {
                case evelo:     // reset velocity values
                    assignToPoint->vx = 0;
                    assignToPoint->vy = 0;
                    break;
                case eacce:     // reset acceleration values
                    assignToPoint->ax = 0;
                    assignToPoint->ay = 0;
                    break;
                case ejerk:     // reset jerk values
                    assignToPoint->jx = 0;
                    assignToPoint->jy = 0;
                    break;
                }
            
            if(point->x != 0)       // valid point
                {
                size_t ind = index % seqsiz; // index into circular buffers

                if(index == 0)
                    // Start accumulation of datapoints
                    {
                    t0 = current->frame;    // first time stamp
                    Ts[ind] = 0;            // time stamp relative to t0
                    }
                else
                    {
                    Ts[ind] = current->frame - t0; // time stamp relative to t0
                    }
                hs[ind] = point->x; // horizontal position
                vs[ind] = point->y; // vertical position
                ++index;            // increase number of collected datapoints
                if(index >= seqsiz) // enough data collected to fill a period
                    {
                    size_t ix; // loop variable
                    double averageTime = 0;

                    double St = 0.0, St2 = 0.0, St3 = 0.0, St4 = 0.0, St5 = 0.0, St6 = 0.0,
                        Sh = 0.0, Sth = 0.0, St2h = 0.0, St3h = 0.0,
                        Sv = 0.0, Stv = 0.0, St2v = 0.0, St3v = 0.0;
                    double averageh = 0; // average horizontal position
                    double averagev = 0; // average vertical position
                    long Tbias = 0;      // average time bias
                    for(ix = 0; ix < seqsiz; ++ix) // compute average time bias
                        {
                        Tbias += Ts[ix];  // sum of time stamps
                        }
                    Tbias /= (long)seqsiz; // average time bias
                    for(ix = index - seqsiz; ix < index; ++ix) // compute averages
                        {
                        size_t id = ix % seqsiz; // index into circular buffers
                        averageh += hs[id];     // sum of horizontal positions
                        averagev += vs[id];     // sum of vertical positions
                        ts[id] = (double)(Ts[id] - Tbias); // This makes ts[id] a small number around 0
                        averageTime += ts[id];  // later used to slightly adjust the ts[id] so that their sum becomes zero.
                        }
                    averageh = averageh / period;       // average horizontal position
                    averagev = averagev / period;       // average vertical position
                    averageTime = averageTime / period; // average of adjusted time stamps
                    for(ix = index - seqsiz; ix < index; ++ix) // compute sums needed for polynomial fitting
                        {
                        size_t id = ix % seqsiz;
                        double t = ts[id] - averageTime;
                        double t2 = t * t;
                        double t3 = t2 * t;
                        double h = hs[id] - averageh;
                        double v = vs[id] - averagev;
                        St += t;
                        St2 += t2;
                        if(St2 > maxSt2)
                            break;
                        St4 += t2 * t2;
                        St5 += t2 * t3;
                        St6 += t3 * t3;
                        Sh += h;
                        Sth += t * h;
                        St2h += t2 * h;
                        St3h += t3 * h;
                        Sv += v;
                        Stv += t * v;
                        St2v += t2 * v;
                        St3v += t3 * v;
                        }
                    if(St2 <= maxSt2)
                        {
                        switch(what)
                            {
                            case evelo:
                                assignToPoint->vx = velocity(St, St2, Sh, Sth, period);
                                assignToPoint->vy = velocity(St, St2, Sv, Stv, period);
                                break;
                            case eacce:
                                assignToPoint->ax = acceleration(St, St2, St3, St4, Sh, Sth, St2h, period);
                                assignToPoint->ay = acceleration(St, St2, St3, St4, Sv, Stv, St2v, period);
                                break;
                            case ejerk:
                                assignToPoint->jx = jerk(St2, St2h, St3, St3h, St4, St5, St6, Sth, period);
                                assignToPoint->jy = jerk(St2, St2v, St3, St3v, St4, St5, St6, Stv, period);
                                break;
                            }
                        }
                    else
                        {
                        //Too many missing observations ('holes') in the current period.
                        }
                    }
                else
                    {
                    //Not enough data collected to fill a period.
                    }
                }
            }
        }
    free(Ts);
    free(ts);
    free(hs);
    free(vs);
    }

line* readInput(char* name)
    {
    FILE* fp;
    fp = fopen(name, "r");
    if(fp)
        {
        int kar;
        int columns = 0;
        int mincols = 99999999;
        int maxcols = 0;
        int cols = 0;
        int maxlen = 0;
        int len = 0;
        while((kar = fgetc(fp)) != EOF)
            {
            ++len;
            if(kar == '\n')
                {
                ++lines;
                if(cols > maxcols)
                    maxcols = cols;
                if(cols < mincols)
                    mincols = cols;
                if(len > maxlen)
                    maxlen = len;
                len = 0;
                cols = 0;
                }
            else if(kar == '\t')
                {
                ++cols;
                }
            }
        if(mincols == maxcols)
            {
            columns = maxcols;
            if(columns != 3 * bodyPoints)
                {
                printf("Body points %d, columns %d\n", bodyPoints, columns);
                exit(2);
                }
            }
        else
            {
            printf("Different number of columns %d and %d\n", mincols, maxcols);
            exit(1);
            }
        rewind(fp);
        // 140	94.7639	90.7244	0.357086	590.125	110.799	0.275333
        line* Lines = new line[lines];
        char* buffer = new char[maxlen + 2];
        int L = 0;
        for(L = 0; L < lines - 1; ++L)
            {
            if(fgets(buffer, maxlen + 1, fp) != 0)
                {
                size_t lngth = strlen(buffer);
                if(lngth > 0)
                    buffer[lngth - 1] = '\t'; // overwrite \n
                char* tab = strchr(buffer, '\t');
                *tab++ = '\0';
                Lines[L].frame = strtol(buffer, 0L, 10);
                for(int i = 0; i < bodyPoints; ++i)
                    {
                    char* nexttab = strchr(tab, '\t');
                    *nexttab = '\0';
                    Lines[L].P[i].x = strtod(tab, 0L);
                    tab = nexttab + 1;

                    nexttab = strchr(tab, '\t');
                    *nexttab = '\0';
                    Lines[L].P[i].y = strtod(tab, 0L);
                    tab = nexttab + 1;

                    nexttab = strchr(tab, '\t');
                    if(nexttab)
                        *nexttab = '\0';
                    Lines[L].P[i].p = strtod(tab, 0L);
                    if(nexttab)
                        tab = nexttab + 1;

                    Lines[L].P[i].vx = 0;
                    Lines[L].P[i].ax = 0;
                    Lines[L].P[i].jx = 0;
                    Lines[L].P[i].vy = 0;
                    Lines[L].P[i].ay = 0;
                    Lines[L].P[i].jy = 0;
                    }
                }
            }
        Lines[L].frame = -1;
        for(int i = 0; i < bodyPoints; ++i)
            {
            Lines[L].P[i].x = 0.0;
            Lines[L].P[i].y = 0.0;
            Lines[L].P[i].p = 0.0;
            Lines[L].P[i].vx = 0;
            Lines[L].P[i].ax = 0;
            Lines[L].P[i].jx = 0;
            Lines[L].P[i].vy = 0;
            Lines[L].P[i].ay = 0;
            Lines[L].P[i].jy = 0;
            }
        fclose(fp);
        return Lines;
        }
    else
        return 0;
    }

int clock(double sagittaH, double sagittaV)
    {
    double theta = atan2((double)-sagittaV, (double)sagittaH) * 180.0 / M_PI;
    theta = -theta;
    if(theta < 0.0)
        theta += 360;
    theta += 105; // half past eleven
    int direction = (int)theta;
    direction %= 360;
    direction /= 30; // 0 .. 11
    if(direction == 0)
        direction = 12;
    return direction;
    }

#if 0
double rho(double sagittaH, double sagittaV)
    {
    double rho = sqrt((double)(sagittaV * sagittaV) + (double)(sagittaH * sagittaH));
    return rho;
    }
#else
#define rho(x,y) hypot(x,y)
#endif

void printBodyPoint(FILE* fp, struct bodyPoint* BodyPoint)
    {
    fprintf(fp,
            "%lf\t%lf\t%lf\t%lf\t%d\t%lf\t%lf\t%lf\t%d\t%lf\t%lf\t%lf\t%d\t%lf\t%lf",
            BodyPoint->x, BodyPoint->y, BodyPoint->p,
            rho(BodyPoint->vx, BodyPoint->vy), clock(BodyPoint->vx, BodyPoint->vy), BodyPoint->vx, BodyPoint->vy,
            rho(BodyPoint->ax, BodyPoint->ay), clock(BodyPoint->ax, BodyPoint->ay), BodyPoint->ax, BodyPoint->ay,
            rho(BodyPoint->jx, BodyPoint->jy), clock(BodyPoint->jx, BodyPoint->jy), BodyPoint->jx, BodyPoint->jy
    );
    // F2:x-pos	F2:y-pos	F2:weight	F2:velocity-r	F2:velocity-clock	F2:velocity-x	F2:velocity-y	F2:acceleration-r	F2:acceleration-clock	F2:acceleration-x	F2:acceleration-y	F2:jerk-r	F2:jerk-clock	F2:jerk-x	F2:jerk-y
    }

void printPointHead(FILE* fp, char* name)
    {
    fprintf(fp
            , "%s:x-pos\t%s:y-pos\t%s:weight\t%s:velocity-r\t%s:velocity-clock\t%s:velocity-x\t%s:velocity-y\t%s:acceleration-r\t%s:acceleration-clock\t%s:acceleration-x\t%s:acceleration-y\t%s:jerk-r\t%s:jerk-clock\t%s:jerk-x\t%s:jerk-y"
            , name, name, name, name, name, name, name, name, name, name, name, name, name, name, name
    );
    }

static char** names;

void print(struct line* Lines, const char* out)
    {
    FILE* fp = fopen(out, "wb");
    if(fp)
        {
        fprintf(fp, "frame\t");
        for(int p = 0; p < bodyPoints; ++p)
            {
            printPointHead(fp, names[p]);
            if(p + 1 < bodyPoints)
                fputc('\t', fp);
            else
                fputc('\n', fp);
            }
        struct line* current = Lines;
        for(current = Lines; current->frame >= 0; ++current)
            {
            fprintf(fp, "%d\t", (current->frame) << 2);
            for(int p = 0; p < bodyPoints; ++p)
                {
                printBodyPoint(fp, current->P + p);
                if(p + 1 < bodyPoints)
                    fputc('\t', fp);
                else
                    fputc('\n', fp);
                }
            }
        fclose(fp);
        }
    }

int main(int argc, char** argv)
    {
    if(argc < 8)
        {
        //       0      1       2          3                    4                  5              6          7       8     9..
        printf("jerk <input> <output> <velocity period> <acceleration period> <jerk period> <margin type> <point> [<point> ...] \n");
        return -1;
        }
    lines = 1;
    bodyPoints = argc - 7;
    names = new char* [bodyPoints];
    for(int i = 0; i < bodyPoints; i++)
        names[i] = argv[7 + i];
    printf("readInput %s\n", argv[1]);
    struct line* Lines = readInput(argv[1]);
    printf("readInput %s DONE\n", argv[1]);
    if(Lines)
        {
        int pnt = 0;
        for(pnt = 0; pnt < bodyPoints; ++pnt)
            {
            doBodyPoint(pnt, evelo, argv[3], argv[6], Lines);
            doBodyPoint(pnt, eacce, argv[4], argv[6], Lines);
            doBodyPoint(pnt, ejerk, argv[5], argv[6], Lines);
            }
        print(Lines, argv[2]);
        free(Lines);
        }
    return 0;
    }
