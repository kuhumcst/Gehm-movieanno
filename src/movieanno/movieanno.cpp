#include <opencv2/opencv.hpp>
#include <string>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>

#include <vector>
#define _USE_MATH_DEFINES
#include <math.h>
/*𝕌𝕋𝔽-𝟠*/
// https://stackoverflow.com/questions/12733105/c-function-that-counts-lines-in-file
#define BUF_SIZE 65536
#define maxcosi -0.8 // threshold cosine of angle between jerk and velocity. MUST be <= 0. If smaller than threshold, we have potential energy
#define energyThreshhold 0.1
#define standardBoundingBoxArea 40000.0
static char staticname[1000];

const cv::Scalar colour_black = cv::Scalar(0, 0, 0);
const cv::Scalar colour_white = cv::Scalar(255, 255, 255);
const cv::Scalar colour_Poten = cv::Scalar(191, 191, 191);
const cv::Scalar colour_strng = cv::Scalar(255, 255, 0);
const cv::Scalar colour_movng = cv::Scalar(0, 255, 255);
const cv::Scalar colour_cursr = cv::Scalar(100, 100, 100);
const cv::Scalar colour_words = cv::Scalar(0, 255, 0);
const cv::Scalar colour_veloc = cv::Scalar(255, 0, 0); // BLUE
const cv::Scalar colour_accel = cv::Scalar(0, 255, 0); // GREEN
const cv::Scalar colour__jerk = cv::Scalar(0, 0, 255); // RED

void myline(cv::InputOutputArray img, cv::Point pt1, cv::Point pt2, const cv::Scalar& color, int thickness)
    {
    if((thickness == 1) || ((thickness & 1) != 1))
        cv::line(img, pt1, pt2, color, thickness);
    else
        {
        int startx1 = pt1.x;
        int startx2 = pt2.x;
        int starty1 = pt1.y;
        int starty2 = pt2.y;
        int thick = thickness / 2;
        if(startx1 == startx2)
            {
            // vertical line
            int x;
            for(x = startx1 - thick; x <= startx1 + thick; ++x)
                {
                pt1.x = x;
                pt2.x = x;
                cv::line(img, pt1, pt2, color, 1);
                }
            }
        else if(starty1 == starty2)
            {
            // vertical line
            int y;
            for(y = starty1 - thick; y <= starty1 + thick; ++y)
                {
                pt1.y = y;
                pt2.y = y;
                cv::line(img, pt1, pt2, color, 1);
                }
            }
        else
            cv::line(img, pt1, pt2, color, thickness);
        }
    }

int count_lines(FILE* file)
    {
    static char buf[BUF_SIZE];
    int counter = 0;
    for(;;)
        {
        size_t res = fread(buf, 1, BUF_SIZE, file);
        if(ferror(file))
            return -1;

        size_t i;
        for(i = 0; i < res; i++)
            if(buf[i] == '\n')
                counter++;

        if(feof(file))
            break;
        }

    return counter;
    }

cv::Scalar anglecolour(double x1, double y1, double x2, double y2)
    {
    x1 = fabs(x1);
    y1 = fabs(y1);
    double r1 = hypot(x1, y1);
    x2 = fabs(x2);
    y2 = fabs(y2);
    double r2 = hypot(x2, y2);
    if(r1 > 0 && r2 > 0)
        {
        x1 = x1 / r1;
        x2 = x2 / r2;
        y1 = y1 / r1;
        y2 = y2 / r2;
        double x = (x1 + x2) / 2.0;
        double y = (y1 + y2) / 2.0;
        double r = hypot(x, y);
        double w = (1.0 - r) / sqrt(3.0);
        x += w;
        y += w;
        r = sqrt(x * x + y * y + w * w);
        assert(r <= 1.0);
        x /= r;
        y /= r;
        w /= r;
        return cv::Scalar((int)round(255.0 * y), (int)round(255.0 * w), (int)round(255.0 * x));
        }
    return cv::Scalar(255, 255, 255);
    }

struct word
    {
    long start;
    long end;
    char word[24]; // first byte y or n
    };

struct speakerdata
    {
    double* energyKinetic;
    double* energyPotential;
    unsigned char* gesture; // 0: no gesture >0:gesture
    int* frame2cs; // frame to centiseconds
    cv::Scalar* coloursE; // energy. Combines directions of v and j
    word* words;
    double topmargin;
    double bottommargin;
    int nwords;
    int nvaj;
    int currentPersonWordIndex;
    std::string VAJ;
    speakerdata(std::string vaj, const char* textgriddata, const char* speaker, long maxframes);
    ~speakerdata()
        {
        delete[] energyKinetic;
        delete[] energyPotential;
        delete[] gesture;
        delete[] frame2cs;
        delete[] coloursE;
        delete[] words;
        }
    void mytimeseries(long count, int frame_width, int frame_height, int horoffset, int itopmargin, cv::Mat* frame, long maxframes, size_t npersons, int incr);
    private:
        speakerdata()
            :
            energyKinetic(0), energyPotential(0), gesture(0), frame2cs(0), coloursE(0), words(0),
            topmargin(0), bottommargin(0), nwords(0), nvaj(0), currentPersonWordIndex(0), VAJ(0)
            {
            }
    };

speakerdata::speakerdata(std::string vaj, const char* textgriddata, const char* speaker, long maxframes)
    :
    energyKinetic(0), energyPotential(0), gesture(0), frame2cs(0), coloursE(0), words(0),
    topmargin(0), bottommargin(0), nwords(0), nvaj(0), currentPersonWordIndex(0), VAJ(vaj)
    {
    FILE* fpvaj = fopen(vaj.c_str(), "rb");
    if(fpvaj == 0)
        {
        printf("Cannot open %s\n", vaj.c_str());
        exit(-1);
        }

    nvaj = count_lines(fpvaj); // FAST
    if(nvaj > 0)
        --nvaj; // do not count head line
    if(maxframes < nvaj)
        nvaj = maxframes;
    printf("iterations %d\n", nvaj);

    rewind(fpvaj);

    long count;

    energyKinetic = new double[nvaj];
    energyPotential = new double[nvaj];
    gesture = new unsigned char[nvaj];
    frame2cs = new int[nvaj]; // frame number to centiseconds conversion
    coloursE = new cv::Scalar[nvaj];
    if(fscanf(fpvaj, " %*[^\n]\n") != 0) // should return 0 since no fields assigned to variables.
        {
        printf("Error reading header line of %s\n", vaj.c_str());
        exit(-1);
        }
    double prevX = 0, prevY = 0, X = 0, Y = 0;
    for(count = 0; count < nvaj; ++count, prevX = X, prevY = Y)
        {
        long frm;
        double xp = 1;
        double yp = 1;
        double velocity_x = 0;
        double velocity_y = 0;
        long   acceleration_clock = 0;
        double acceleration_x = 0;
        double acceleration_y = 0;
        double jerk_x = 0;
        double jerk_y = 0;
        double maxf = 0;
        int ret = fscanf(fpvaj,
                         "%ld %lf %lf %*f %*f %*d %lf %lf %*f %ld %lf %lf %*f %*d %lf %lf",
                         &frm,
                         &xp,
                         &yp,
                         &velocity_x,
                         &velocity_y,
                         &acceleration_clock,
                         &acceleration_x,
                         &acceleration_y,
                         &jerk_x,
                         &jerk_y);
        if(xp == 0 || yp == 0)
            {
            velocity_x = 0;
            velocity_y = 0;
            acceleration_clock = 0;
            acceleration_x = 0;
            acceleration_y = 0;
            jerk_x = 0;
            jerk_y = 0;
            }
        if(ret != 10)
            break;
        assert(frm != 0 || count < 20);
        frame2cs[count] = frm;

        int skip;
        for(skip = 14; skip; --skip) // Skip 14 OpenPose key points, until REye.
            {
            ret = fscanf(fpvaj, "%*f %*f %*f %*f %*d %*f %*f %*f %*d %*f %*f %*f %*d %*f %*f");
            }
        double minx, maxx, miny, maxy;

        // Try to define a bounding box {minx,miny,maxx,maxy} by checking positions of eyes and ears
        // If any position is 0, discard all data of the frame.
        //  REye
        ret = fscanf(fpvaj,
                     "%lf %lf %*f %*f %*d %*f %*f %*f %*d %*f %*f %*f %*d %*f %*f",
                     &minx,
                     &miny);
        maxx = minx;
        maxy = miny;
        if(ret != 2)
            break;

        //  LEye
        ret = fscanf(fpvaj,
                     "%lf %lf %*f %*f %*d %*f %*f %*f %*d %*f %*f %*f %*d %*f %*f",
                     &xp,
                     &yp);
        if(ret != 2)
            break;
        if(xp != 0 && (minx > xp || minx == 0)) minx = xp;
        if(yp != 0 && (miny > yp || miny == 0)) miny = yp;
        if(xp != 0 && (maxx < xp || maxx == 0)) maxx = xp;
        if(yp != 0 && (maxy < yp || maxy == 0)) maxy = yp;

        //  REar
        ret = fscanf(fpvaj,
                     "%lf %lf %*f %*f %*d %*f %*f %*f %*d %*f %*f %*f %*d %*f %*f",
                     &xp,
                     &yp);
        if(ret != 2)
            break;
        if(xp == 0 || yp == 0) // Right ear not found
            {
            energyKinetic[count] = -1;
            energyPotential[count] = 0;
            gesture[count] = 0;
            coloursE[count] = 0;
            //  LEar
            ret = fscanf(fpvaj, "%*[^\n]\n"); // Skip left ear
            }
        else
            {
            double weight2 = 1;
            if(minx > xp) minx = xp;
            if(miny > yp) miny = yp;
            if(maxx < xp) maxx = xp;
            if(maxy < yp) maxy = yp;

            //  LEar
            ret = fscanf(fpvaj,
                         "%lf %lf %*[^\n]\n",
                         &xp,
                         &yp);
            if(ret != 2)
                break;
            if(xp == 0 || yp == 0) // Left ear not found
                {
                energyKinetic[count] = -1;
                energyPotential[count] = 0;
                gesture[count] = 0;
                coloursE[count] = 0;
                }
            else
                {
                if(minx > xp) minx = xp;
                if(miny > yp) miny = yp;
                if(maxx < xp) maxx = xp;
                if(maxy < yp) maxy = yp;

                X = maxx - minx;
                Y = maxx - minx; // Intentional. Assume square box containing ears and eyes. 
                if(prevX == 0)
                    prevX = X;
                if(prevY == 0)
                    prevY = Y;

                X = (4.0 * prevX + X) / 5.0;
                Y = (4.0 * prevY + Y) / 5.0;
                double XY2 = X * X + Y * Y;

                // Scaling: persons sitting close to the camera get a lower weight
                // than persons sitting further away, so their graphics become comparable.
                // The value 40000 (200x200 pixels bounding box for "standard" face) is determined heuristically
                if(XY2 > 0)
                    {
                    weight2 = standardBoundingBoxArea / XY2;
                    }

                double cos3 = 1.0;
                double val = velocity_x * acceleration_x + velocity_y * acceleration_y;
                double v_r = hypot(velocity_x, velocity_y);
                double a_r = hypot(acceleration_x, acceleration_y);
                double j_r = hypot(jerk_x, jerk_y);
                if(v_r > 0 && a_r > 0 && val != 0)
                    cos3 = val / (v_r * a_r); // cosine of the angle between acceleration and velocity
                assert(cos3 <= 1.05);
                assert(cos3 >= -1.05);
                if(cos3 > 1)
                    cos3 = 1;
                if(cos3 < -1)
                    cos3 = -1;

                val = jerk_x * acceleration_x + jerk_y * acceleration_y;
                if(j_r > 0 && a_r > 0)
                    cos3 *= val / (j_r * a_r); // cosine of the angle between acceleration and jerk 
                                               // times
                                               // cosine of the angle between acceleration and velocity
                assert(cos3 <= 1.05);
                assert(cos3 >= -1.05);
                if(cos3 > 1)
                    cos3 = 1;
                if(cos3 < -1)
                    cos3 = -1;

                double kinenergy = weight2 * (v_r * v_r);
                if(kinenergy >= INT32_MAX)
                    printf("%s[%ld] %f weight2 %f v_r %f velocity_x %f velocity_y %f\n", speaker, count, kinenergy, weight2, v_r, velocity_x, velocity_y);
                energyKinetic[count] = kinenergy;

                energyPotential[count] = 0;
                gesture[count] = 0;
                val = velocity_x * jerk_x + velocity_y * jerk_y;
                if(j_r > 0 && v_r > 0 && val != 0)
                    {
                    cos3 *= val / (v_r * j_r); // The more aligned jerk and velocity, the higher the weight.
                                               // cosine of the angle between acceleration and jerk 
                                               // times
                                               // cosine of the angle between acceleration and velocity
                                               // times
                                               // cosine of the angle between velocity and jerk
                    if(val < 0)
                        {
                        double cosi = val / (v_r * j_r); // cosine of the angle between velocity and jerk
                        if(cosi < maxcosi) // jerk and velocity opposite, so oscillating
                                           // cosi MUST be < 0
                                           // We take the sqrt of -cosi !
                            {
                            double omegasquare = -val / (v_r * v_r); // seconds^-2
#define maxhertz 4.0 // Rough estimate of max number of head movements per second.
                            if(omegasquare > ((2.0 * M_PI) / 25.0) * ((2.0 * M_PI) / 25.0) * maxhertz * maxhertz)
                                omegasquare = ((2.0 * M_PI) / 25.0) * ((2.0 * M_PI) / 25.0) * maxhertz * maxhertz;
                            double omega = sqrt(omegasquare);
                            double f = (25.0 / (2.0 * M_PI)) * omega; // 25 small time units in one second
                            if(f > maxf)
                                maxf = f;
                            energyPotential[count] = weight2 * a_r * a_r / omegasquare; // meters^2 * seconds^-2
                            if(energyKinetic[count] + energyPotential[count] > energyThreshhold)
                                {
                                gesture[count] = 1;
                                if(count >= 2)
                                    {
                                    if(gesture[count - 1] == 0 && energyPotential[count - 1] <= 0.0 && energyPotential[count - 2] > 0.0)
                                        {
                                        // If the current frame has very low energy but neighbouring frames have potential energy,
                                        // then give the current frame a potential energy that is the mean of the total energies
                                        // of the neighbouring frames.
                                        // This is meant to solve the problem that v/j is badly defined if v is very small or zero.
                                        // In that regime, the inner product of v and j is noisy and the potential energy may come out as zero.
                                        energyPotential[count - 1] = 0.5 * (energyKinetic[count - 2] + energyKinetic[count] + energyPotential[count - 2] + energyPotential[count]);
                                        if(energyKinetic[count - 1] + energyPotential[count - 1] > energyThreshhold)
                                            {
                                            gesture[count - 1] = 1;
                                            }
                                        }
                                    }

                                }
                            }
                        }
                    }
                assert(cos3 <= 1.05);
                assert(cos3 >= -1.05);
                if(cos3 > 1)
                    cos3 = 1;
                if(cos3 < -1)
                    cos3 = -1;


                cos3 = fabs(cos3);
                val *= cos3; // diminish the product of velocity and jerk once more before storing!
                if(val > topmargin)
                    topmargin = val;
                if(val < bottommargin)
                    bottommargin = val;
                val = fabs(val);

                val = acceleration_x * acceleration_x + acceleration_y * acceleration_y;
                val *= cos3; // diminish the square of acceleration before storing!
                if(-val > topmargin)
                    topmargin = -val;
                if(val < bottommargin)
                    bottommargin = -val;
                val = fabs(val);

                coloursE[count] = anglecolour(velocity_x, velocity_y, jerk_x, jerk_y);
                }
            }
        }
    fclose(fpvaj);
        // check
    std::string fnam = std::string(speaker) + "-framd2cs";

    FILE* fpframe2cs = fopen(fnam.c_str(), "w");
    if(fpframe2cs)
        {
        for(int k = 0; k < count; ++k)
            {
            fprintf(fpframe2cs, "%d %d\n", k, frame2cs[k]);
            }
        fclose(fpframe2cs);
        }

    if(textgriddata)
        {
        printf("textgriddata %s speaker %s\n", textgriddata, speaker);
        FILE* fp = fopen(textgriddata, "rb");
        if(fp)
            {  // Fill words array
            char buffer[1024];
            for(; fgets(buffer, sizeof(buffer), fp);)
                {
                char* found = strstr(buffer, speaker);
                if(found == buffer)
                    {
                    ++nwords;
                    }
                }
            words = new word[nwords];
            rewind(fp);
            int i = 0;
            count = 0;
            for(; i < nwords && fgets(buffer, sizeof(buffer), fp); )
                {
                char* found = strstr(buffer, speaker);
                if(found == buffer)
                    {
                    words[i].word[0] = buffer[6];
                    int start = 0, end = 0;
                    int t = sscanf(buffer + 8, "%d %d %23s", &start, &end, words[i].word + 1);
                    while(count < nvaj && frame2cs[count] + 2 < start)
                        ++count;
                    if(count < nvaj)
                        words[i].start = count;
                    else
                        words[i].start = start / 4; // estimate
                    while(count < nvaj && frame2cs[count] < end)
                        ++count;
                    if(count < nvaj)
                        words[i].end = count;
                    else
                        words[i].end = end / 4; // estimate
                    if(t != 3)
                        break;
                    ++i;
                    }
                }
            fclose(fp);
            // check
            fp = fopen(speaker, "wb");
            if(fp)
                {
                for(int ix = 0; ix < nwords; ++ix)
                    {
                    fprintf(fp, "%ld %ld %s\n", words[ix].start, words[ix].end, words[ix].word);
                    }
                fclose(fp);
                }
            }
        }
    }


std::vector<std::tuple< std::string, std::string, std::string>> listmovies(char* name, char* speaker)
    {
    std::vector<std::tuple< std::string, std::string, std::string>> lst;
    const std::filesystem::path sandbox{ name };
//    std::filesystem::create_directories(sandbox / "dir1" / "dir2");
//    std::ofstream{ sandbox / "file1.txt" };
//    std::ofstream{ sandbox / "file2.txt" };

    std::cout << "directory_iterator:" << std::endl;
    // directory_iterator can be iterated using a range-for loop
    for(auto const& dir_entry : std::filesystem::directory_iterator{ sandbox })
        {
        //std::cout << dir_entry.path().filename() << " " << dir_entry.path().filename().stem() << " " << dir_entry.path().filename().extension() << std::endl;
        std::u8string stem = dir_entry.path().filename().stem().u8string();
        std::string sname(stem.cbegin(), stem.cend());
        if(!strncmp(sname.c_str(), "20", 2)) // Must start with century == 20, not a fucking .- Apple invention.
            {
            std::cout << "Name: " << sname.substr(9) << "name.substr(9,5):" << sname.substr(9, 5) << std::endl;
            if(speaker == 0 || sname.find(speaker) != std::string::npos)
                {
                std::u8string extension = dir_entry.path().filename().extension().u8string();
                std::string ext(extension.cbegin(), extension.cend());
                std::tuple< std::string, std::string, std::string> pair(sname, sname.substr(9, 5), ext);// skip yyyymmdd and hyphen
                lst.push_back(pair);
                }
            }
        }
/*
    std::cout << "\ndirectory_iterator as a range:\n";
    // directory_iterator behaves as a range in other ways, too
    std::ranges::for_each(
        std::filesystem::directory_iterator{ sandbox },
        [](const auto& dir_entry) { std::cout << dir_entry << '\n'; });

    std::cout << "\nrecursive_directory_iterator:\n";
    for(auto const& dir_entry : std::filesystem::recursive_directory_iterator{ sandbox })
        std::cout << dir_entry << '\n';
        */
    // delete the sandbox dir and all contents within it, including subdirs
  //  std::filesystem::remove_all(sandbox);
    return lst;
    }

std::string listmovie(char* name)
    {
    const std::filesystem::path sandbox{ name };
    std::string ret;
    for(auto const& dir_entry : std::filesystem::directory_iterator{ sandbox })
        {
        std::u8string stem = dir_entry.path().u8string();
        std::string sname(stem.cbegin(), stem.cend());
        ret = sname;
        }
    return ret;
    }

void speakerdata::mytimeseries(long count, int frame_width, int frame_height, int horoffset, int itopmargin, cv::Mat* frame, long maxframes, size_t npersons, int incr)
    {
    int pixelspermovieframe = 5;
    int startmovieframe = count - horoffset / pixelspermovieframe;
    if(startmovieframe < 0)
        {
        startmovieframe = 0;
        }
    int endmovieframe = startmovieframe + frame_width / pixelspermovieframe;
    if(maxframes > nvaj)
        maxframes = nvaj;
    if(endmovieframe > maxframes)
        endmovieframe = maxframes;
    int x = 0;
    if(npersons < 2)
        cv::rectangle(*frame, cv::Point((frame_width * 3) / 4, 0), cv::Point(frame_width, frame_height), colour_white, -1);
    for(int i = startmovieframe; i < endmovieframe; ++i, x += pixelspermovieframe)
        {
        cv::Scalar colour_E = coloursE[i];


        int ibottommargin = frame_height;
        bool movement = false;
        if(energyPotential[i] > 0)
            {
            double M = 0;
            int k = i - 4;
            int l = i + 4;
            if(k < 0)
                {
                l = l - k;
                k = 0;
                }
            else if(l >= nvaj)
                {
                k = k - (l - nvaj + 1);
                l = nvaj - 1;
                }
            for(int m = k; m <= l; ++m)
                {
                M += energyPotential[m] + energyKinetic[m];
                }
            M /= l - k + 1; // M is mean kinetic plus potential energy over 9 frames
            if(M >= energyThreshhold)
                {
                movement = true;
                double ekin = energyKinetic[i];
                double epot = energyPotential[i];
                if(M * 25.0 < incr || npersons < 2)
                    {
                    ekin *= 10;
                    epot *= 10;
                    }
                else
                    { // Large movement that won't fit if not scaled down.
                      // Draw line above that indicates bars have 10 times the shown strength.
                    myline(*frame, cv::Point(x, itopmargin - 5), cv::Point(x, itopmargin - 6), colour_strng, pixelspermovieframe);
                    }

                int h_omegaP = (int)(round(epot));
                if(ekin > 0)
                    {
                    int h_omegak = (int)(round(ekin));
                    int yK = itopmargin + h_omegak;
                    if(ibottommargin < yK)
                        yK = ibottommargin;
                    myline(*frame, cv::Point(x, itopmargin), cv::Point(x, yK), colour_E, pixelspermovieframe);

                    int yP = itopmargin + h_omegak + h_omegaP;
                    if(ibottommargin < yP)
                        {
                        yP = ibottommargin;
                        }
                    myline(*frame, cv::Point(x, yK), cv::Point(x, yP), colour_Poten, pixelspermovieframe);
                    }
                else
                    {
                    int yP = itopmargin + h_omegaP;
                    if(ibottommargin < yP)
                        {
                        yP = ibottommargin;
                        h_omegaP = ibottommargin - itopmargin;
                        }
                    myline(*frame, cv::Point(x, itopmargin), cv::Point(x, yP), colour_Poten, pixelspermovieframe);
                    }
                }
            }
        else if(energyKinetic[i] > 0)
            {
            double M = 0;
            int k = i - 4;
            int l = i + 4;
            if(k < 0)
                {
                l = l - k;
                k = 0;
                }
            else if(l >= nvaj)
                {
                k = k - (l - nvaj + 1);
                l = nvaj - 1;
                }
            for(int m = k; m <= l; ++m)
                {
                M += energyKinetic[m];
                }
            M /= l - k + 1; // M is mean kinetic energy over 9 frames
            if(M > energyThreshhold)
                {
                movement = true;
                double ekin = energyKinetic[i];
                if(M * 25.0 < incr || npersons < 2) // Do not scale down strong signal if there is only one person
                    {
                    ekin *= 10;
                    }
                else
                    { // Large movement that won't fit if not scaled down.
                      // Draw line above that indicates bars have 10 times the shown strength.
                    myline(*frame, cv::Point(x, itopmargin - 5), cv::Point(x, itopmargin - 6), colour_strng, pixelspermovieframe);
                    }

                int h_omega = (int)(round(ekin));
                int y = itopmargin + h_omega;
                if(ibottommargin < y)
                    {
                    y = ibottommargin;
                    }
                myline(*frame, cv::Point(x, itopmargin), cv::Point(x, y), colour_E, pixelspermovieframe);
                }
            }
        else if(energyKinetic[i] < 0.0)
            {
            myline(*frame, cv::Point(x, itopmargin - 5), cv::Point(x, itopmargin - 6), colour_black, pixelspermovieframe);
            myline(*frame, cv::Point(x, itopmargin - 7), cv::Point(x, itopmargin - 8), colour_white, pixelspermovieframe);
            }
        if(movement)
            {
            myline(*frame, cv::Point(x, itopmargin - 7), cv::Point(x, itopmargin - 8), colour_movng, pixelspermovieframe);
            }
        if(npersons < 2)
            {
            if((count == x / pixelspermovieframe + startmovieframe))
                myline(*frame, cv::Point(x, frame_height), cv::Point(x, 0), colour_black, 1);
            }
        else
            {
// grey cursor line
            if((count == x / pixelspermovieframe + startmovieframe))
                myline(*frame, cv::Point(x, 2 * itopmargin), cv::Point(x, 0), colour_cursr, 3);
            }
        }

    if(words) // From TextGrid input
        {
        for(; currentPersonWordIndex < nwords && words[currentPersonWordIndex].end < startmovieframe; ++currentPersonWordIndex)
            ;
        int w = currentPersonWordIndex;
        for(; w < nwords && words[w].start < endmovieframe; ++w)
            {
            int start = words[w].start;
            if(start < startmovieframe)
                start = startmovieframe;
            int end = words[w].end;
            if(end > endmovieframe)
                end = endmovieframe;
            int x1 = (start - startmovieframe) * pixelspermovieframe;
            int x2 = (end - startmovieframe) * pixelspermovieframe;
            int y = itopmargin;
            if(x1 < x2)
                {
                int fb = (words[w].word[0] == 'y') ? 11 : 3;
                myline(*frame, cv::Point(x1, y), cv::Point(x2, y), colour_words, fb);
                }
            }
        w = currentPersonWordIndex;
        for(; w < nwords && words[w].start < endmovieframe; ++w)
            {
            int start = words[w].start;
            if(start < startmovieframe)
                start = startmovieframe;
            int end = words[w].end;
            if(end > endmovieframe)
                end = endmovieframe;
            int x1 = (start - startmovieframe) * pixelspermovieframe;
            int x2 = (end - startmovieframe) * pixelspermovieframe;
            int y = itopmargin;
            if(x1 < x2)
                {
                if(words[w].word[0] == 'y')
                    cv::putText(*frame, words[w].word + 1, cv::Point(x1 - 4, y + 5), cv::FONT_HERSHEY_SIMPLEX, 0.4, colour_black, 1);
                }
            }
        }
    }

bool exists(const char* name)
    {
    std::filesystem::path Path{ name };
    return std::filesystem::exists(Path);
    }

std::string u8(std::filesystem::path Path)
    {
    std::u8string Path8 = Path.u8string();
    std::string String8(Path8.cbegin(), Path8.cend());
    return String8;
    }

std::filesystem::path FinalDatasetRoot(const char* yyyymmdd)
    {
    std::filesystem::path Root(".");

    if(std::filesystem::exists(Root / yyyymmdd))
        return Root;

    Root = "FinalDataset";
    if(std::filesystem::exists(Root / yyyymmdd))
        return Root;

    printf("Folder %s that contains SeparatedSpeakersVideo and OriginalVideo does not exist. Exiting.\n", yyyymmdd);
    exit(-1);
    }

bool checkTextGridAvailable(const char* yyyymmdd)
    {
    std::filesystem::path Praat{ "praat" };
    std::filesystem::path HtmlPath = Praat / "html";
    std::filesystem::path TabPath = HtmlPath / "tab";
    sprintf(staticname, "%s_Praat_long.TextGrid.html.tab", yyyymmdd);
    std::filesystem::path TextGridTabSeparated = TabPath / staticname; // e.g. praat/html/tab/20210323_Praat_long.TextGrid.html.tab
    bool TextGridAvailable = std::filesystem::exists(TextGridTabSeparated);
    if(TextGridAvailable)
        return true;
    std::filesystem::path Stem_html = TextGridTabSeparated.filename().stem(); // 20210323_Praat_long.TextGrid.html
    std::filesystem::path YYYYMMDD{ yyyymmdd }; // e.g. 20210323
    std::filesystem::path Html = HtmlPath / Stem_html; // praat/html/20210323_Praat_long.TextGrid.html
    std::u8string Html8 = Html.u8string();
    std::string HTMLname(Html8.cbegin(), Html8.cend());
    std::filesystem::path Stem_TextGrid = Stem_html.stem(); // 20210323_Praat_long.TextGrid
    std::filesystem::path TextGrid = YYYYMMDD / Stem_TextGrid; // 20210323/20210323_Praat_long.TextGrid
    std::u8string TextGrid8 = TextGrid.u8string();
    std::string TextGridName(TextGrid8.cbegin(), TextGrid8.cend());

    if(!std::filesystem::exists(TextGrid)) // e.g. 20220916/20220916_Praat_long.TextGrid
        {
        std::string sname(TextGrid8.cbegin(), TextGrid8.cend());
        printf("1 File %s does not exist.\nContinue? (y/n)\n", sname.c_str());
        int c;
        do c = getchar(); while(c != 'y' && c != 'n');
        if(c == 'n')
            exit(-1);
        }

    if(!std::filesystem::exists(Html))  // e.g. praat/html/20220916_Praat_long.TextGrid.html
        {
        std::u8string sHtml8 = Html.u8string();
        std::string sHTMLname(sHtml8.cbegin(), sHtml8.cend());
        if(!std::filesystem::exists(TextGrid))
            {
            printf("2 File %s does not exist and cannot be created because %s does not exist either.\n", sHTMLname.c_str(), TextGridName.c_str());
            }
        else
            {
#ifdef __linux__ 
    //linux code goes here
            std::string command("bracmat \"get\\$\\\"feedback.bra\\\"\" ");  // Creates HTML output, combines all speakers.
            command += '"' + TextGridName + '"';
#elif _WIN32
    // windows code goes here
            std::string command("bracmat \"get$\\\"feedback.bra\\\"\" ");  // Creates HTML output, combines all speakers.
            command += '"' + TextGridName + '"';
#else
#error Do not know the OS
#endif
            std::cout << command.c_str() << std::endl;
            if(system(command.c_str()) != 0)
                {
                printf("Cannot execute command \"%s\"\n", command.c_str());
                return false;
                }
            }
        }

    if(!std::filesystem::exists(TextGridTabSeparated))
        {
        std::u8string TextgridData8 = TextGridTabSeparated.u8string();
        std::string TextGridTabSeparatedName(TextgridData8.cbegin(), TextgridData8.cend());

        if(std::filesystem::exists(Html))
            {
#ifdef __linux__ 
    //linux code goes here
            std::string command("bracmat \"get\\$\\\"speakseries.bra\\\"\" ");
            command += '"' + HTMLname + '"';
#elif _WIN32
    // windows code goes here
            std::string command("bracmat \"get$\\\"speakseries.bra\\\"\" ");
            command += '"' + HTMLname + '"';
#else
#error Do not know the OS
#endif
            /* Creates tab separated file
            Like this:
            SP01F n -100 -100 30
            SP07F n -100 -100 15
            SP01F y 343  367  Yes
            SP01F n 515  545  so
            SP01F n 577  593  we
            SP01F n 597  617  had
            ...
            */
            std::cout << command.c_str() << std::endl;
            if(system(command.c_str()) != 0)
                {
                printf("Cannot execute command \"%s\"\n", command.c_str());
                return false;
                }
            }
        else
            {
            printf("5 File %s does not exist and cannot be created because %s does not exist.\n",
                   TextGridTabSeparatedName.c_str(), HTMLname.c_str());
            return false;
            }
        }
    return true;
    }

int createTabFromJSON(std::filesystem::path Root, const char* yyyymmdd)
    {
#ifdef __linux__ 
    //linux code goes here
    std::string command("bracmat \"get\\$\\\"jsn2tab.bra\\\"\" ");  // Creates tabulated output from OpenPose JSON files.
    command += u8(Root) + " " + yyyymmdd;
#elif _WIN32
    // windows code goes here
    std::string command("bracmat \"get$\\\"jsn2tab.bra\\\"\" ");  // Creates tabulated output from OpenPose JSON files.
    command += u8(Root) + " " + yyyymmdd;
#else
#error Do not know the OS
#endif
    std::cout << command.c_str() << std::endl;
    return system(command.c_str());
    }

int createVAJ(const char* yyyymmdd, const char* windows)
    {
#ifdef __linux__ 
    //linux code goes here
    std::string command("bracmat \"get\\$\\\"jerk.bra\\\"\" ");  // Creates HTML output, combines all speakers.
    command = command + yyyymmdd + " " + windows; // instead of 9 11 13
#elif _WIN32
    // windows code goes here
    std::string command("bracmat \"get$\\\"jerk.bra\\\"\" ");  // Creates HTML output, combines all speakers.
    command = command + yyyymmdd + " " + windows; // instead of 9 11 13
#else
#error Do not know the OS
#endif
    std::cout << command.c_str() << std::endl;
    return system(command.c_str());
    }

double getSpeakerData(size_t numberOfSpeakers, std::filesystem::path Root, speakerdata** pdata, const char* yyyymmdd, const char* windows, std::vector<std::tuple<std::string, std::string, std::string>> list, long maxframes)
    {
    size_t icurrentspeaker = 0;
    double topmargin = 0;
    bool TextGridAvailable = checkTextGridAvailable(yyyymmdd);
    for(auto it = begin(list); it != end(list); ++it, ++icurrentspeaker)
        {
        if(icurrentspeaker < numberOfSpeakers)
            {
            std::string stem; // stem
            std::string stemfrom9; // stem minus first 9 chars == speaker
            std::string extension; // including period
            stemfrom9 = std::get<1>(*it); // another way to access an element that is not the first. (LISPish)
            std::cout << "stemfrom9:" << stemfrom9 << std::endl;
            sprintf(staticname, "vaj/%s-%s.vaj/%s-%s_keypoints-%s.vaj.tab", yyyymmdd, windows, yyyymmdd, stemfrom9.c_str(), windows);
            std::string vaj = staticname;
            std::cout << "vaj:" << vaj << std::endl;
            if(!exists(vaj.c_str()))
                {
                std::filesystem::path KeypointsTab("keypoints.tab");
                KeypointsTab = KeypointsTab / yyyymmdd / (std::string(yyyymmdd) + "-" + stemfrom9 + "_keypoints.tab");
                if(!exists(KeypointsTab))
                    {
                    printf("%s does not exist. Is it zipped?\n... Going to try to create it from OpenPose JSON output in FinalDataset folder. (Which may be zipped too!)\n", u8(KeypointsTab).c_str());
                    createTabFromJSON(Root, yyyymmdd);
                    if(!exists(KeypointsTab))
                        {
                        printf("Cannot create %s. Exiting.\n", u8(KeypointsTab).c_str());
                        exit(-2);
                        }
                    }
                createVAJ(yyyymmdd, windows);
                }
            if(!exists(vaj.c_str()))
                {
                std::cout << "vaj(2):" << vaj << std::endl;
                printf("----->Velocity, acceleration and jerk data are not found. Exiting\n");
                exit(-1);
                }
            sprintf(staticname, "praat/html/tab/%s_Praat_long.TextGrid.html.tab", yyyymmdd);
            pdata[icurrentspeaker] = new speakerdata(vaj, (TextGridAvailable ? staticname : 0), stemfrom9.c_str(), maxframes);
            if(topmargin < pdata[icurrentspeaker]->topmargin)
                topmargin = pdata[icurrentspeaker]->topmargin;
            }
        }
    return topmargin;
    }

long dostuff(std::filesystem::path Root, const char* yyyymmdd, const char* windows, std::vector<std::tuple<std::string, std::string, std::string>> list, std::string inputPath, std::string outputPath, long maxframes)
    {
    // Open input video
    cv::VideoCapture cap(inputPath);

    if(!cap.isOpened())
        {
        std::cerr << "Error: Cannot open input video file " << inputPath << std::endl;
        return 0;
        }

    // Get video properties
    int frame_width = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
    int frame_height = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
    int frame_count = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_COUNT));
    printf("frame_count %d\n", frame_count);
    if(maxframes < 0 || frame_count < maxframes)
        maxframes = frame_count;
    double fps = cap.get(cv::CAP_PROP_FPS);
    int fourcc = static_cast<int>(cap.get(cv::CAP_PROP_FOURCC));

    // Open output video writer
    cv::VideoWriter writer(outputPath, fourcc, fps, cv::Size(frame_width, frame_height));
    if(!writer.isOpened())
        {
        std::cerr << "Error: Cannot open output video file." << std::endl;
        return 0;
        }
    cv::Mat frame;

    size_t numberOfSpeakers = list.size();
    if(numberOfSpeakers > 0)
        {
        speakerdata** pdata = new speakerdata * [numberOfSpeakers];
        double topmargin = getSpeakerData(numberOfSpeakers, Root, pdata, yyyymmdd, windows, list, maxframes);
        printf("topmargin %f\n", topmargin);
        size_t icurrentspeaker = 0;
        icurrentspeaker = 0;
        for(auto it = begin(list); it != end(list); ++it, ++icurrentspeaker)
            {
            if(icurrentspeaker < numberOfSpeakers)
                {
                double factor = frame_height / 25.0;
                pdata[icurrentspeaker]->topmargin = factor;
                pdata[icurrentspeaker]->bottommargin *= factor / pdata[icurrentspeaker]->topmargin;
                }
            }

        int itopmargin = frame_height / 10;

        printf("itopmargin = %d\n", itopmargin);

        int horoffset = (numberOfSpeakers > 1) ? (frame_width * 9) / 10 : (frame_width * 7) / 8; // position of cursor within frame, in pixels
        printf("horoffset %d ('hot' pixel x )\n", horoffset);

        int incr = frame_height / (int)numberOfSpeakers;
        if(numberOfSpeakers < 2)
            {
            int wv = 0, wa = 0, wj = 0;
            char* endptr = 0;
            wv = strtol(windows, &endptr, 10);
            if(*endptr == '-')
                {
                wa = strtol(endptr + 1, &endptr, 10);
                if(*endptr == '-')
                    {
                    wj = strtol(endptr + 1, &endptr, 10);
                    }
                }
            const double scale = 1.0;
            const double deltav = wv;
            const double deltaa = wa;
            const double deltaj = wj;
            printf("deltav %f deltaa %f deltaj %f\n", deltav, deltaa, deltaj);
            FILE* fpvaj = 0;
            fpvaj = fopen(pdata[0]->VAJ.c_str(), "r");
            if(fpvaj == 0)
                {
                printf("Cannot open %s\n", pdata[0]->VAJ.c_str());
                exit(-4);
                }
            if(fscanf(fpvaj, " %*[^\n]\n") != 0)
                {
                printf("Error reading from %s\n", pdata[0]->VAJ.c_str());
                exit(-4);
                }
            long count;
            printf("Starting iteration over frames\n");
            setbuf(stdout, NULL);
            for(count = 0; cap.read(frame) && count < maxframes; ++count)
                {
                if((count % 100) == 0)
                    printf("count %ld \r", count);
                if(count < pdata[0]->nvaj) // There may be fewer OpenCV data than frames. A Zoom participant may leave early.
                    {
                    // Add line overlay
                    long f;
                    double xp = 1;
                    double yp = 1;
                    double velocity_x = 0;
                    double velocity_y = 0;
                    double acceleration_x = 0;
                    double acceleration_y = 0;
                    double jerk_x = 0;
                    double jerk_y = 0;
                    int ret = fscanf(fpvaj,
                                     "%ld %lf %lf %*f %*f %*d %lf %lf %*f %*d %lf %lf %*f %*d %lf %lf %*[^\n]\n",
                                     &f,
                                     &xp,
                                     &yp,
                                     &velocity_x,
                                     &velocity_y,
                                     &acceleration_x,
                                     &acceleration_y,
                                     &jerk_x,
                                     &jerk_y
                    );
                    if(ret != 9)
                        break;
                    if(xp != 0 && yp != 0)
                        {
                        if(pdata[0]->energyKinetic[count] > 0.0)
                            {
                            double xv = xp + scale * deltav * velocity_x;
                            double xa = xv + scale * deltav * deltaa * acceleration_x;
                            double xj = xa + scale * deltav * deltaa * deltaj * jerk_x;
                            double yv = yp + scale * deltav * velocity_y;
                            double ya = yv + scale * deltav * deltaa * acceleration_y;
                            double yj = ya + scale * deltav * deltaa * deltaj * jerk_y;
                            myline(frame, cv::Point((int)xp, (int)yp), cv::Point((int)(xv), (int)(yv)), colour_veloc, 3);
                            myline(frame, cv::Point((int)xv, (int)yv), cv::Point((int)(xa), (int)(ya)), colour_accel, 3);
                            myline(frame, cv::Point((int)xa, (int)ya), cv::Point((int)(xj), (int)(yj)), colour__jerk, 3);
                            }
                        }
                    }
                pdata[0]->mytimeseries(count, frame_width, frame_height, horoffset, itopmargin, &frame, maxframes, numberOfSpeakers, incr);
                // Write frame to output
                writer.write(frame);
                }
            printf("Final count %ld \n", count);
            if(fpvaj)
                fclose(fpvaj);

            if(maxframes == 0)
                exit(-5);
            }
        else
            {
            itopmargin = incr / 7;
            if(itopmargin > frame_height / 10)
                itopmargin = frame_height / 10;
            long count = 0;
            for(count = 0; cap.read(frame) && count < maxframes; ++count)
                {
                if((count % 100) == 0)
                    printf("count %ld \r", count);
                icurrentspeaker = 0;
                for(auto it = begin(list); it != end(list); ++it, ++icurrentspeaker)
                    {
                    // Add line overlay
                    if(icurrentspeaker < numberOfSpeakers)
                        pdata[icurrentspeaker]->mytimeseries(count, frame_width, frame_height, horoffset, itopmargin + (int)icurrentspeaker * incr, &frame, maxframes, numberOfSpeakers, incr);

                    if(maxframes == 0)
                        exit(-6);
                    }
                // Write frame to output
                writer.write(frame);
                }
            printf("Final count %zu\n", icurrentspeaker);
            printf("Speakers %ld \n", count);
            }
        printf("Loop done.\n");
        cap.release();
        printf("cap.release() done\n");
        writer.release();
        printf("writer.release() done\n");

        icurrentspeaker = 0;
        for(auto it = begin(list); it != end(list); ++it, ++icurrentspeaker)
            {
            if(icurrentspeaker < numberOfSpeakers)
                delete pdata[icurrentspeaker];
            }
        delete[] pdata;
        printf("dostuff DONE\n");
        }
    return 0;
    }


int main(int argc, char* argv[])
    {
    const char* yyyymmdd = "20230310";
    long maxframes = -1;
    char locname[1000];
    printf("argc %d\n", argc);
    const char* windows = "9-11-13";
    char* speaker = 0;
    if(argc > 1)
        {
        yyyymmdd = argv[1];
        if(argc > 2)
            {
            char* endptr = 0;
            // movieanno date maxframes
            // If maxframes > 0, annotate max n frames and shorten movie to maxframes.
            // If maxframes <= 0, copy all frames to output, annotate as many as possible.
            maxframes = strtol(argv[2], &endptr, 10);
            if(*endptr == '-')
                {
                maxframes = -1; // default
                windows = argv[2]; // e.g. 5-7-9
                if(argc > 3)
                    {
                    maxframes = strtol(argv[3], &endptr, 10);
                    if(*endptr != 0)
                        {
                        // movieanno date windows name
                        maxframes = -1; // default
                        speaker = argv[3];
                        }
                    else if(argc > 4)
                        {
                        // movieanno date windows n name
                        // Load separate speaker video
                        speaker = argv[4];
                        }
                    }
                }
            else if(*endptr == 0)
                {
                // Load all speakers video
                if(argc > 3)
                    {
                    // movieanno date n name
                    // Load separate speaker video
                    speaker = argv[3];
                    }
                }
            else
                {
                // movieanno date name
                // Load separate speaker video
                speaker = argv[2];
                }
            }
        }
    else
        {
        printf("Date %s\nmovieanno yyyymmdd [#v-#a-#j] [#frames] [speaker]\n", __DATE__);
        return -1;
        }

    std::filesystem::path Root = FinalDatasetRoot(yyyymmdd);

    sprintf(locname, "%s", u8((Root / yyyymmdd / "SeparatedSpeakersVideo")).c_str()); // From contents of folder we can count number of speakers.
                                                          // That number is part of the output file name,
                                                          // also if only one, specific, speaker is shown.
    if(!exists(locname))
        {
        printf("Folder %s does not exist.\n", locname);
        exit(-1);
        }
    std::vector<std::tuple<std::string, std::string, std::string>> list = listmovies(locname, speaker);
    if(list.size() == 0)
        {
        sprintf(locname, "%s", u8((Root / yyyymmdd / "OpenPoseKeypoints" / yyyymmdd / yyyymmdd)).c_str()); // From contents of folder we can count number of speakers.
        if(!exists(locname))
            sprintf(locname, "%s", u8((Root / yyyymmdd / "OpenPoseKeypoints" / yyyymmdd)).c_str()); // From contents of folder we can count number of speakers.
        if(exists(locname))
            {
            list = listmovies(locname, speaker);
            }
        }
    if(list.size() == 0)
        {
        if(speaker)
            {
            printf("There is no speaker '%s'\n", speaker);
            }
        else
            {
            printf("There are no speakers\n");
            }
        exit(-1);
        }
    std::string inputMovie;
    if(speaker == 0)
        {// User did not specify a speaker, so show all speakers. (And Do not show the overlayed velocity, acceleration and jerk vectors.)
        sprintf(locname, "%s", u8((Root / yyyymmdd / "OriginalVideo")).c_str());
        inputMovie = listmovie(locname);
        }
    else
        { //User specified a speaker. Show everything for this speaker, and nothing for the other speakers.
        std::string stem;
        std::string extension; // including period
        stem = std::get<0>(*begin(list));
        extension = std::get<2>(begin(list)[0]);  // one way to access an element that is not the first
        sprintf(locname, "%s%s", u8((Root / yyyymmdd / "SeparatedSpeakersVideo" / stem)).c_str(), extension.c_str()); // a == stem
        inputMovie = locname;
        }
    std::cout << "inputMovie:" << inputMovie << std::endl;

    std::string outputPath;
    const char* speakerName = "all";
    size_t numberOfSpeakers = list.size();
    if(numberOfSpeakers == 1)
        speakerName = speaker;
    sprintf(locname, "%s-%s-%s-%ld-%zu.with.overlay.mov",
            yyyymmdd, windows, speakerName, maxframes, numberOfSpeakers);
    outputPath = locname;
    maxframes = dostuff(Root, yyyymmdd, windows, list, inputMovie, outputPath, maxframes);

    return 0;
    }
