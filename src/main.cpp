#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <fstream>
#include <unordered_map>

using namespace cv;
using namespace std;

int x1_, y1_;   // topleft x,y
int x2_, y2_;   // bottom  x,y
float half_sz_; // half length of bounding box
Mat imgOrg;     // color screenshot

string stage_;                      // stage
int megaProcess_;                   // mega process
unordered_map<string, string> pkm_; // pokemon name and icon file <pokemon name, icon path>
vector<pair<string, string>> team_; // pokemons into the board <pokemon name, icon path>
unordered_map<string, int> megaTh_; // mega threshold <pokemon name, threshold>

void SplitString(const string& s, vector<string>& v, const string& c)
{
    string::size_type pos1, pos2;
    pos2 = s.find(c);
    pos1 = 0;
    while(string::npos != pos2)
    {
        v.push_back(s.substr(pos1, pos2 - pos1));

        pos1 = pos2 + c.size();
        pos2 = s.find(c, pos1);
    }
    if(pos1 != s.length())
        v.push_back(s.substr(pos1));
}

bool LoadConfig()
{
    std::ifstream ifs("../config.txt", std::ifstream::in);
    if(!ifs.is_open())
        return false;

    ifs >> x1_ >> y1_ >> x2_ >> y2_ >> half_sz_;
    ifs.close();
    return true;
}

bool LoadTeam()
{
    {
        std::ifstream ifs("../img/icons.txt", std::ifstream::in);
        if(!ifs.is_open())
            return false;
        while(!ifs.eof())
        {
            string strSkip, pkmName, pkmIcon;
            ifs >> strSkip >> pkmName >> pkmIcon;
            pkm_[pkmName] = pkmIcon;
        }
    }

    ifstream fileTeam("../team.txt", ifstream::in);
    if(!fileTeam.is_open())
        return false;

    cout << "Load Stage and Team..." << endl;
    fileTeam >> stage_;
    cout << " * Stage: " << stage_ << endl;

    string strTeams;
    fileTeam >> strTeams;
    vector<string> vTeams;
    SplitString(strTeams, vTeams, ",");

    bool bMega = false;
    if(megaTh_.find(vTeams[0]) != megaTh_.end())
    {
        int th = megaTh_[vTeams[0]];
        if(megaProcess_ >= th)
        {
            megaProcess_ = th;
            bMega = true;
        }
        cout << " * Mega Process " << megaProcess_ << "/" << th << endl;
    }
    else
        megaProcess_ = 0;

    int nPkm = 0;
    for(auto str : vTeams)
    {
        nPkm++;
        if(nPkm == 1 && bMega)
        {
            team_.emplace_back(str, pkm_["Mega_" + str]);
            cout << " - " << str << " " << pkm_["Mega_" + str] << " *" << endl;
        }
        else
        {
            team_.emplace_back(str, pkm_[str]);
            cout << " - " << str << " " << pkm_[str] << endl;
        }
    }
    team_.emplace_back("Wood", "img/wood.png");
    return true;
}

bool LoadMegaThresh()
{
    std::ifstream ifs("../img/effects.txt", std::ifstream::in);
    if(!ifs.is_open())
        return false;

    string str;
    const size_t nskip = sizeof("INTEGER MEGA_THRESHOLD_Mega_") - 1;
    while(getline(ifs, str))
    {
        size_t pos = str.find("MEGA_THRESHOLD");
        if(pos != string::npos)
        {
            str.erase(0, nskip);
            vector<string> vstrs;
            SplitString(str, vstrs, " ");
            megaTh_[vstrs[0]] = atoi(vstrs[1].c_str());
        }
    }
    return true;
}

void GenerateIconCenter(vector<Point2f>& centers)
{
    vector<float> vX(6), vY(6);
    const float dx = (x2_ - x1_) / 5.0f;
    const float dy = (y2_ - y1_) / 5.0f;
    for(int i = 0; i < 6; i++)
    {
        vX[i] = x1_ + dx * i;
        vY[i] = y1_ + dy * i;
    }

    for(int j = 0; j < 6; j++)
        for(int i = 0; i < 6; i++)
            centers.emplace_back(vX[i], vY[j]);
}

void ExtractIconFeature(const vector<Point2f>& centers, vector<vector<KeyPoint>>& keypoints, vector<Mat>& descriptors)
{
    Mat imgFeature = imgOrg.clone();
    Mat grayImage, grayImageCLAHE;
    cvtColor(imgOrg, grayImage, CV_BGR2GRAY);
    Mat mask(grayImage.size(), CV_8UC1);
    Ptr<CLAHE> clahe = createCLAHE(4.0);
    clahe->apply(grayImage, grayImageCLAHE);

    const float sz = half_sz_;
    for (int i = 0; i < 36; ++i)
    {
        Point2f center = centers[i];
        Point2f tl = center - Point2f(sz, sz);
        Point2f br = center + Point2f(sz, sz);

        // set mask
        mask = Scalar(0);
        mask.rowRange(tl.y, br.y).colRange(tl.x, br.x) = Scalar(255);

        // extract keypoints
        vector<Point2f> pts;
        goodFeaturesToTrack(grayImage, pts, 50, 0.01, 5, mask);
        if(pts.size() < 20)
            goodFeaturesToTrack(grayImageCLAHE, pts, 50, 0.01, 4, mask);

        vector<KeyPoint> kpts;
        KeyPoint::convert(pts, kpts);
        for(KeyPoint& kp : kpts)
            kp.angle = 0.0;

        // feature description
        Mat desc;
        ORB orb(500, 1.2f, 1, 11);
        orb.compute(grayImage, kpts, desc);
        // cout << pts.size() << "  " << desc.size() << endl;

        keypoints.push_back(kpts);
        descriptors.push_back(desc);

        cv::rectangle(imgFeature, tl, br, Scalar(0, 0, 255));
        drawKeypoints(imgFeature, kpts, imgFeature);
    }

    // imshow("Feature", imgFeature);
    // waitKey(0);
}

// ref: He, Kaiming, Jian Sun, and Xiaoou Tang. "Single image haze removal using dark channel prior."
Mat GetDarkChannel(const Mat& src)
{
    Mat rgbmin = Mat::zeros(src.rows, src.cols, CV_8UC1);
    Mat dark = Mat::zeros(src.rows, src.cols, CV_8UC1);

    for (int m = 0; m < src.rows; m++)
    {
        for (int n = 0; n < src.cols; n++)
        {
            const Vec3b& intensity = src.at<Vec3b>(m, n);
            rgbmin.at<uchar>(m, n) = min(min(intensity.val[0], intensity.val[1]), intensity.val[2]);
        }
    }

    const int priorSize = 5;
    erode(rgbmin, dark, Mat::ones(priorSize, priorSize, CV_8UC1));
    return dark;
}

int main(int argc, char** argv)
{
    if(argc < 2)
    {
        cout << "Usage: ./ShuffleMoveHelper screenshot.jpeg [mega progress]" << endl;
        return 0;
    }

    if(argc == 2)
        megaProcess_ = 0;
    else
        megaProcess_ = atoi(argv[2]);

    // load config
    if(!LoadConfig())
        return 0;

    // load Mega threshold
    if(!LoadMegaThresh())
        return 0;

    // load Team
    if(!LoadTeam())
        return 0;

    // load image
    const string fileName(argv[1]);
    imgOrg = imread(fileName);
    if(imgOrg.empty())
    {
        cout << "error loading file " << fileName << endl;
        return 0;
    }
    cv::resize(imgOrg, imgOrg, Size(), 0.4, 0.4);

    std::vector<Point2f> vIconCenters;
    std::vector<vector<KeyPoint>> vKeypoints;
    std::vector<Mat> vDescriptors;

    GenerateIconCenter(vIconCenters);
    ExtractIconFeature(vIconCenters, vKeypoints, vDescriptors);

    // 1. identify board, each kind of pokemon is represented by numbers (class id)
    int idPokemon = 1;
    std::vector<int> vClass(36, 0);
    for(int i = 0; i < 36; i++)
    {
        if(vClass[i] != 0)
            continue;

        // the metal has a few keypoints
        if(vKeypoints[i].size() <= 8)
        {
            vClass[i] = 9;
            continue;
        }

        // assign a new pokemon id
        vClass[i] = idPokemon++;

        for(int j = i + 1; j < 36; j++)
        {
            if(vClass[j] != 0)
                continue;

            const int idxQuery = i;
            const int idxMatch = j;
            const auto& kptsQuery = vKeypoints[idxQuery];
            const auto& kptsMatch = vKeypoints[idxMatch];
            const auto& descQuery = vDescriptors[idxQuery];
            const auto& descMatch = vDescriptors[idxMatch];

            std::vector<DMatch> matches;
            BFMatcher matcher(NORM_HAMMING);
            matcher.match(descQuery, descMatch, matches);

            std::vector<char> vbGoodMatches;
            for(const auto& m : matches)
            {
                // if descriptors' distance is less than 80 and keypoints' distance is less than 3
                // it should be a good match
                bool bGoodMatch = m.distance < 80;
                Point2f ptQuery = kptsQuery[m.queryIdx].pt - vIconCenters[idxQuery];
                Point2f ptMatch = kptsMatch[m.trainIdx].pt - vIconCenters[idxMatch];
                Point2f dpt = ptQuery - ptMatch;
                bGoodMatch &= (norm(dpt) < 8.0);

                vbGoodMatches.push_back(bGoodMatch);
            }

            std::vector<int> vIdxInMatches;
            std::vector<Point2f> ptsQuery, ptsMatch;
            for(size_t k = 0, kend = matches.size(); k < kend; k++)
            {
                if(!vbGoodMatches[k])
                    continue;
                vIdxInMatches.push_back(k);
                ptsQuery.push_back(kptsQuery[matches[k].queryIdx].pt);
                ptsMatch.push_back(kptsMatch[matches[k].trainIdx].pt);
            }
            if(vIdxInMatches.size() <= 6)
                continue;

            vector<uchar> status;
            Mat H = findHomography(ptsQuery, ptsMatch, status, CV_RANSAC, 3);

            int nInliers = 0;
            vector<char> vbInliers = vbGoodMatches;
            for(size_t k = 0, kend = status.size(); k < kend; k++)
            {
                vbInliers[vIdxInMatches[k]] = status[k];
                nInliers += status[k];
            }
            // cout << "Match: " << kptsQuery.size() << " " << kptsMatch.size()
            //      << ", Inliers: " << nInliers << endl;

            if(nInliers >= 10)
                vClass[j] = vClass[i];

            // Mat imgMatches;
            // drawMatches(imgOrg, kptsQuery, imgOrg, kptsMatch, matches, imgMatches,
            //             Scalar::all(-1), Scalar::all(-1), vbInliers);
            // imshow("Result", imgMatches);
            // waitKey(0);
        }
    }

    // check the range of class id
    for(auto id : vClass)
        if(id > 9)
            return 0;

    // print board
    cout << "Game board: " << endl;
    for(int i = 0; i < 36; i++)
    {
        cout << vClass[i] << " ";
        if((i + 1) % 6 == 0)
            cout << endl;
    }

    // 2. assign pokemon name to class id and identify barriers
    Mat imgDark = GetDarkChannel(imgOrg);

    std::vector<bool> vbBarrier(36, false);
    std::vector<int> vCntPerClass(10, 0);
    std::vector<string> vstrClass(10, "Air");
    vstrClass[9] = "Metal"; // class id 9 is assigned to Metal
    for(const auto& team : team_)
    {
        const string pkm = team.first;
        const string icon = "../" + team.second;

        Mat imgIconColor = imread(icon);
        resize(imgIconColor, imgIconColor, Size(), 0.65, 0.65);

        Mat imgIcon;
        cvtColor(imgIconColor, imgIcon, CV_BGRA2GRAY);
        Mat imgIconDark = GetDarkChannel(imgIconColor);

        // extract keypoints
        vector<Point2f> pts;
        goodFeaturesToTrack(imgIcon, pts, 100, 0.01, 5);

        vector<KeyPoint> kpts;
        KeyPoint::convert(pts, kpts);
        for(KeyPoint& kp : kpts)
            kp.angle = 0.0;

        // feature description
        Mat descriptors;
        ORB orb(500, 1.2f, 1, 5);
        orb.compute(imgIcon, kpts, descriptors);
        // cout << kpts.size() << "  " << descriptors.size() << endl;

        // Mat imgFeature;
        // drawKeypoints(imgIcon, kpts, imgFeature);
        // imshow("Feature", imgFeature);
        // waitKey(0);

        std::vector<int> vDarkChannelDiff(36, -1);
        unordered_map<int, int> vHist; // <class id, count>
        for(int i = 0; i < 36; i++)
        {
            if(vClass[i] == 9)
                continue;

            BFMatcher matcher(NORM_HAMMING);
            std::vector<DMatch> matches;
            matcher.match(descriptors, vDescriptors[i], matches);

            std::vector<char> vbGoodMatches;
            for(const auto& m : matches)
            {
                // if descriptors' distance is less than 80 and keypoints' distance is less than 3
                // it should be a good match
                bool bGoodMatch = m.distance < 80;
                Point2f ptQuery = kpts[m.queryIdx].pt - Point2f(imgIcon.rows / 2.0f, imgIcon.rows / 2.0f);
                Point2f ptMatch = vKeypoints[i][m.trainIdx].pt - vIconCenters[i];
                Point2f dpt = ptQuery - ptMatch;
                bGoodMatch &= (norm(dpt) < 16.0);

                vbGoodMatches.push_back(bGoodMatch);
            }

            std::vector<int> vIdxInMatches;
            std::vector<Point2f> ptsQuery, ptsMatch;
            for(size_t k = 0, kend = matches.size(); k < kend; k++)
            {
                if(!vbGoodMatches[k])
                    continue;
                vIdxInMatches.push_back(k);
                ptsQuery.push_back(kpts[matches[k].queryIdx].pt);
                ptsMatch.push_back(vKeypoints[i][matches[k].trainIdx].pt);

            }
            if(vIdxInMatches.size() < 5)
                continue;

            vector<uchar> status;
            Mat H = findHomography(ptsQuery, ptsMatch, status, CV_RANSAC, 3);

            int nInliers = 0;
            vector<char> vbInliers = vbGoodMatches;
            for(size_t k = 0, kend = status.size(); k < kend; k++)
            {
                vbInliers[vIdxInMatches[k]] = status[k];
                nInliers += status[k];
            }
            // cout << "Match: " << kpts.size() << " " << vKeypoints[i].size()
            //      << ", Inliers: " << nInliers << endl;

            if(nInliers >= 10)
                vHist[vClass[i]]++;

            // if(nInliers >= 10)
            {
                int sum = 0;
                for(size_t k = 0, kend = vbInliers.size(); k < kend; k++)
                {
                    if(!vbInliers[k])
                        continue;
                    int pixIcon = imgIconDark.at<uchar>(kpts[matches[k].queryIdx].pt);
                    int pixImg = imgDark.at<uchar>(vKeypoints[i][matches[k].trainIdx].pt);
                    sum += (pixImg - pixIcon);
                }
                vDarkChannelDiff[i] = sum / (float)nInliers;
            }

            // Mat img_matches;
            // drawMatches(imgIconColor, kpts, imgOrg, vKeypoints[i], matches, img_matches,
            //             Scalar::all(-1), Scalar::all(-1), vbInliers);
            // imshow("Result", img_matches);
            // waitKey(0);
        }

        int nMax = -1;
        int classId = -1;
        for(const auto& it : vHist)
        {
            if(it.second > nMax)
            {
                classId = it.first;
                nMax = it.second;
            }
        }

        if(nMax > vCntPerClass[classId])
        {
            vstrClass[classId] = pkm;
            vCntPerClass[classId] = nMax;
        }

        for(int i = 0; i < 36; i++)
        {
            if(vClass[i] == classId)
            {
                vbBarrier[i] = vDarkChannelDiff[i] > 80;
            }
        }
    }

    // 3. save board
    {
        std::vector<string> vbBarrierStr(36);
        for(int i = 0; i < 36; i++)
            vbBarrierStr[i] = vbBarrier[i] ? "true" : "false";

#if defined(__linux__) || defined(__APPLE__)
        string fileBoard = string(getenv("HOME")) + "/Shuffle-Move/config/boards/board.txt";
#else
        string fileBoard = "../board.txt";
#endif
        cout << "Save board.txt to " << fileBoard << endl;
        std::ofstream ofs(fileBoard, std::ofstream::out);

        ofs << "STAGE " << stage_ << endl;
        ofs << "MEGA_PROGRESS " << megaProcess_ << endl;
        ofs << "STATUS NONE" << endl;
        ofs << "STATUS_DURATION 0" << endl;

        for(int r = 0; r < 6; r++)
        {
            ofs << "ROW_" << to_string(r + 1) << " ";
            for(int c = 0; c < 5; c++)
                ofs << vstrClass[vClass[r * 6 + c]] << ",";
            ofs << vstrClass[vClass[r * 6 + 5]] << endl;

            ofs << "FROW_" << to_string(r + 1) << " ";
            for(int c = 0; c < 5; c++)
                ofs << vbBarrierStr[r * 6 + c] << ",";
            ofs << vbBarrierStr[r * 6 + 5] << endl;

            ofs << "CROW_" << to_string(r + 1) << " false,false,false,false,false,false" << endl;
        }
        ofs.close();
    }
}
