#include <fstream>
#include <opencv2/opencv.hpp>

using namespace cv;
using namespace std;

int x1_ = 2000;
int y1_ = 2000;
int x2_ = 0;
int y2_ = 0;
float half_sz_ = 26.0f;
Mat imgOrg, imgRst;

void onMouse(int event, int x, int y, int flags, void* userdata)
{
    if (event == CV_EVENT_LBUTTONDOWN)
    {
        x1_ = min(x1_, x);
        y1_ = min(y1_, y);
        x2_ = max(x2_, x);
        y2_ = max(y2_, y);
        cout << "[ " << x << ", " << y << " ]" << endl;
        cv::circle(imgRst, Point(x, y), 3, Scalar(0, 0, 255));
        imshow("ShuffleMove", imgRst);
    }
}

int main(int argc, char** argv)
{
    if(argc < 2)
        return 0;

    const string fileName(argv[1]);
    Mat imgOrg = imread(fileName);
    if(imgOrg.empty())
    {
        cout << "error loading file " << fileName << endl;
        return 0;
    }

    cv::resize(imgOrg, imgOrg, Size(), 0.4, 0.4);
    imgRst = imgOrg.clone();

    if(argc == 2)
    {
        namedWindow("ShuffleMove");
        setMouseCallback("ShuffleMove", onMouse, 0);
        imshow("ShuffleMove", imgOrg);
        waitKey(0);

        printf("%d %d %d %d\n", x1_, y1_, x2_, y2_);
        std::ofstream ofs("../config.txt", std::ofstream::out);
        ofs << x1_ << " " << y1_ << " " << x2_ << " " << y2_ << " "
            << half_sz_ << endl;
        ofs.close();
    }
    else if(argc == 3)
    {
        std::ifstream ifs("../config.txt", std::ifstream::in);
        ifs >> x1_ >> y1_ >> x2_ >> y2_ >> half_sz_;
        ifs.close();
        printf("%d %d %d %d %f\n", x1_, y1_, x2_, y2_, half_sz_);

        float dx = (x2_ - x1_) / 5.0f;
        float dy = (y2_ - y1_) / 5.0f;
        vector<float> vX(6), vY(6);
        for(int i = 0; i < 6; i++)
        {
            vX[i] = x1_ + dx * i;
            vY[i] = y1_ + dy * i;
        }

        const float sz = half_sz_;
        for(int j = 0; j < 6; j++)
            for(int i = 0; i < 6; i++)
            {
                Point2f center(vX[i], vY[j]);
                Point tl = center - Point2f(sz, sz);
                Point br = center + Point2f(sz, sz);

                cv::circle(imgRst, center, 2, Scalar(0, 0, 255), 2);
                cv::rectangle(imgRst, tl, br, Scalar(0, 0, 255));
            }
        imshow("Boundary", imgRst);
        waitKey(0);
    }

    return 0;
}
