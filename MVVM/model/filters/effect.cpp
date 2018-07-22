#include "effect.h"
#include <opencv2/opencv.hpp>
#include <string>
#include <cstring>

using namespace std;

void effect::_sharpen(const cv::Mat &scr, cv::Mat &dst)
{
    cv::Mat kernel(3,3,CV_32F,cv::Scalar(0));
    kernel.at<float>(1,1) = 5;
    kernel.at<float>(0,1) = -1;
    kernel.at<float>(2,1) = -1;
    kernel.at<float>(1,0) = -1;
    kernel.at<float>(1,2) = -1;
    cv::filter2D(scr,dst,scr.depth(),kernel);
    dst.row(0).setTo(cv::Scalar(0,0,0));
    dst.row(dst.rows-1).setTo(cv::Scalar(0,0,0));
    dst.col(0).setTo(cv::Scalar(0,0,0));
    dst.col(dst.cols-1).setTo(cv::Scalar(0,0,0));
}

void effect::_inversecolor(const cv::Mat &scr, cv::Mat &dst)
{
    cv::Mat temp;
    temp = 255 -scr;
    dst = temp.clone();
}

void effect::_colortoblack(const cv::Mat &scr, cv::Mat &dst)
{
    cv::Mat temp_1;

    cv::cvtColor(scr,temp_1,CV_BGR2GRAY);
    cv::cvtColor(temp_1,dst,cv::COLOR_GRAY2BGR);
}

void effect::_defog(const cv:: Mat &scr,cv::Mat &dst)
{
    cv::Mat c;
    cv::Mat b = scr.clone();
    defog a;
    c = a.DeFog(b);
    dst = c.clone();
    a.~defog();
}

cv::Mat defog::DeFog(cv::Mat scr)
{
    cv::Mat a = darkChannelDefog(scr);
    cv::Mat b = getTImage();
    cv::Mat c = enhanceImage(a);
    return a;
}

defog::~defog()
{

}

cv::Mat defog::minRGB(cv::Mat src)
{
    cv::Mat minRgb;
    if(src.empty())
        return minRgb;

    minRgb= cv::Mat::zeros(src.rows,src.cols,CV_8UC1);
    for(int i=0;i<src.rows;i++)
        for(int j=0;j<src.cols;j++)
        {
            uchar g_minvalue =255;
            for(int c=0;c<3;c++)
            {
                if(g_minvalue>src.at<cv::Vec3b>(i,j)[c])
                    g_minvalue=src.at<cv::Vec3b>(i,j)[c];
            }
            minRgb.at<uchar>(i,j)=g_minvalue;
        }
    return minRgb;
}

cv::Mat defog::minFilter(cv::Mat src, int ksize)
{
    cv::Mat dst;
    if(src.channels()!=1)
        return dst  ;
    if(src.depth()>8)
        return dst;

    int r=(ksize-1)/2;

    dst=cv::Mat::zeros(src.rows,src.cols,CV_8UC1);


    for(int i=0;i<src.rows;i++)
        for(int j=0;j<src.cols;j++)
        {
            int top=i-r;
            int bottom=i+r;
            int left=j-r;
            int right=j+r;
            if(i-r<0)
                top=0;
            if(i+r>src.rows)
                bottom=src.rows;
            if(j-r<0)
                left=0;
            if(j+r>src.cols)
                right=src.cols;
            cv::Mat ImROI=src(cv::Range(top,bottom),cv::Range(left,right));
            double min,max;
            cv::minMaxLoc(ImROI,&min,&max,0,0);
            dst.at<uchar>(i,j)=min;
        }
    return dst;
}

cv::Mat defog::grayStretch(const cv::Mat src, double lowcut, double highcut)
{
    const int bins = 256;
    int hist_size=bins;
    float range[]={0,255};
    const float* ranges[]={range};
    cv::MatND desHist;
    int channels=0;
    cv::calcHist(&src,1,&channels,cv::Mat(),desHist,1,&hist_size,ranges,true,false);
    int pixelAmount=src.rows*src.cols;
    float Sum=0;
    int minValue,maxValue;
    for(int i=0;i<bins;i++)
    {
        Sum=Sum+desHist.at<float>(i);
        if(Sum>=pixelAmount*lowcut*0.01)
        {
            minValue=i;
            break;
        }
    }

    Sum=0;
    for(int i=bins-1;i>=0;i--)
    {
        Sum=Sum+desHist.at<float>(i);
        if(Sum>=pixelAmount*highcut*0.01)
        {
            maxValue=i;
            break;
        }
    }

    cv::Mat dst=src;
    if(minValue>maxValue)
        return src;

    for(int i=0;i<src.rows;i++)
        for(int j=0;j<src.cols;j++)
        {
            if(src.at<uchar>(i,j)<minValue)
                dst.at<uchar>(i,j)=0;
            if(src.at<uchar>(i,j)>maxValue)
                dst.at<uchar>(i,j)=255;
            else
            {
                double pixelValue=((src.at<uchar>(i,j)-minValue)/
                                   (double)(maxValue-minValue))*255;
                dst.at<uchar>(i,j)=(int)pixelValue;
            }
        }


    return dst;
}

cv::Mat defog::darkChannelDefog(cv::Mat src)
{
    cv::Mat tempImage=minRGB(src);

    int ksize=std::max(3,std::max((int)(src.cols*kernRatio),
                                  (int)(src.rows*kernRatio)));
    tempImage=minFilter(tempImage,ksize);

    m_tImage=cv::Mat::zeros(src.rows,src.cols,CV_64FC1);
    for(int i=0;i<src.rows;i++)
        for(int j=0;j<src.cols;j++)
            m_tImage.at<double>(i,j)=((255.0-
                                       (double)tempImage.at<uchar>(i,j)*wFactor)/255)-0.005;

    double A[3];cv::Point maxLoc;
    cv::minMaxLoc(tempImage,0,0,0,&maxLoc);
    for(int c=0;c<src.channels();c++)
        A[c]=src.at<cv::Vec3b>(maxLoc.y,maxLoc.x)[c];
    m_dstImage=cv::Mat::zeros(src.rows,src.cols,CV_64FC3);
    for(int i=0;i<src.rows;i++)
        for(int j=0;j<src.cols;j++)
            for(int c=0;c<src.channels();c++)
                m_dstImage.at<cv::Vec3d>(i,j)[c]=(src.at<cv::Vec3b>(i,j)[c]-
                                              (1-m_tImage.at<double>(i,j))*A[c])/
                        std::max(m_tImage.at<double>(i,j),min_t);
    m_dstImage.convertTo(m_dstImage,CV_8UC3);


    return m_dstImage;

}

cv::Mat defog::enhanceImage(cv::Mat src)
{
    cv::Mat dst;
    cv::Mat channels[3];
    cv::split(src,channels);
    for(int c=0;c<3;c++)
        channels[c]= grayStretch(channels[c],0.001,1);
    cv::merge(channels,3,dst);

    return dst;
}

cv::Mat defog::getTImage()
{
    cv::Mat temp = cv::Mat::zeros(m_tImage.rows,m_tImage.cols,CV_8UC1);
    for(int i=0;i<m_tImage.rows;i++)
        for( int j=0;j<m_tImage.cols;j++)
        {
            temp.at<uchar>(i,j) = (int)(m_tImage.at<double>(i,j)*255);
        }
    m_tImage.convertTo(m_tImage,CV_8UC1);
    m_tImage = temp;
    return m_tImage;
}

void effect::_balance(const cv::Mat &scr,cv::Mat &dst)
{
    cv::Mat image = dst.clone();
    cv::Mat mergeImg;


    vector<cv::Mat> splitBGR(image.channels());

    cv::split(image,splitBGR);


    for(int i=0; i<image.channels(); i++)

        cv::equalizeHist(splitBGR[i],splitBGR[i]);


    cv::merge(splitBGR,mergeImg);
    dst = mergeImg.clone();
}

void effect::_soft(const cv::Mat &scr,cv::Mat &dst)
{
    cv::Mat temp = scr.clone();
    cv::GaussianBlur(temp,dst,cv::Size(1,1),0);
}

void effect::_nostalgia(const cv::Mat &scr,cv::Mat &dst)
{

    int rowNum = scr.rows;
    int colNum = scr.cols;
    dst = scr.clone();
    for(int j = 0;j<rowNum;j++){
        uchar* data = dst.ptr<uchar>(j);
        for(int i = 0;i<colNum;i++){
            int b = data[i*3];
            int g = data[i*3+1];
            int r = data[i*3+2];

            int R = static_cast<int>(0.393*r + 0.769*g + 0.189*b);
            int G = static_cast<int>(0.349*r + 0.686*g + 0.168*b);
            int B = static_cast<int>(0.272*r + 0.534*g + 0.131*b);

            data[i*3+2] = max(0,min(R,255));
            data[i*3+1] = max(0,min(G,255));
            data[i*3] = max(0,min(B,255));
        }
    }
}

void effect::_BlackComic(const cv::Mat &scr,cv::Mat &dst)
{
    int rowNum = scr.rows;
     int colNum = scr.cols;
     dst = scr.clone();

     for(int j = 0;j<rowNum;j++){
         uchar* data = dst.ptr<uchar>(j);
         for(int i = 0;i<colNum;i++){
             int b = data[i*3];
             int g = data[i*3+1];
             int r = data[i*3+2];

             int R = (abs(2*g-b+r)*r)>>8;
             int G = (abs(2*b-g+r)*r)>>8;
             int B = (abs(2*b-g+r)*r)>>8;

             R = max(0,min(R,255));
             G = max(0,min(G,255));
             B = max(0,min(B,255));

             int gray = (R+G+B)/3;

             R = min(255,gray + 10);

             data[i*3+2] = R;
             data[i*3+1] = R;
             data[i*3] = gray;
         }
     }
}

void effect::_timetuunel(const cv::Mat &scr,cv::Mat &dst)
{
cv::Mat Img_in = scr.clone();
cv::Mat temp = Img_in.clone();
cv::Point center(Img_in.cols/2,Img_in.rows/2);
cv::Mat Img_out(Img_in.size(), CV_32FC3);
Img_in.convertTo(Img_out, CV_32FC3);
int width=Img_in.cols;
int height=Img_in.rows;
float R;
float angle;
cv::Point Center(width/2, height/2);
float t1, t2, t3;
int new_x, new_y;
int Num=20;
for (int y=0; y<height; y++)
{
    for (int x=0; x<width; x++)
    {
        t1=0; t2=0; t3=0;
        R=sqrt((y-Center.y)*(y-Center.y)+(x-Center.x)*(x-Center.x));
        angle=atan2((float)(y-Center.y), (float)(x-Center.x));
        for (int mm=0; mm<Num; mm++)
        {
            float tmR=R-mm>0 ? R-mm : 0.0;
            new_x=tmR*cos(angle)+Center.x;
            new_y=tmR*sin(angle)+Center.y;
            if(new_x<0)       new_x=0;
            if(new_x>width-1) new_x=width-1;
            if(new_y<0)       new_y=0;
            if(new_y>height-1)new_y=height-1;
            t1=t1+Img_in.at<cv::Vec3b>(new_y, new_x)[0];
            t2=t2+Img_in.at<cv::Vec3b>(new_y, new_x)[1];
            t3=t3+Img_in.at<cv::Vec3b>(new_y, new_x)[2];
        }
        Img_out.at<cv::Vec3f>(y, x)[0]=t1/Num;
        Img_out.at<cv::Vec3f>(y, x)[1]=t2/Num;
        Img_out.at<cv::Vec3f>(y, x)[2]=t3/Num;
    }
}
int radius = min(temp.cols,temp.rows);
radius = radius / 3;
for ( int x = 0; x < temp.cols; x++)
{
    for ( int y = 0; y < temp.rows; y++)
    {
        int m = ((x - center.x) * (x - center.x) + (y - center.y) *(y - center.y));
        if (m < (radius * radius))
        {
            Img_out.at<cv::Vec3f>(Point(x, y))[0] = temp.at<cv::Vec3b>(cv::Point(x, y))[0];
            Img_out.at<cv::Vec3f>(Point(x, y))[1] = temp.at<cv::Vec3b>(cv::Point(x, y))[1];
            Img_out.at<cv::Vec3f>(Point(x, y))[2] = temp.at<cv::Vec3b>(cv::Point(x, y))[2];
        }
    }
}
Img_out=Img_out/255.0;

}

void effect::_classiclomo(const cv::Mat &scr,cv::Mat &dst)
{
    cv::Mat image= scr.clone();
    cv::Mat new_image = cv::Mat::zeros( image.size(), image.type() );
    for( int y = 0; y < image.rows; y++ )
    {
        for( int x = 0; x < image.cols; x++ )
        {
                new_image.at<cv::Vec3b>(y,x)[0] =  10*sqrt(image.at<cv::Vec3b>(y,x)[0]);
                new_image.at<cv::Vec3b>(y,x)[1] = image.at<cv::Vec3b>(y,x)[1];
                new_image.at<cv::Vec3b>(y,x)[2] =  image.at<cv::Vec3b>(y,x)[2];
        }
    }
    cv::Mat mergeImg;
    vector<cv::Mat> splitBGR(image.channels());
    cv::split(image,splitBGR);
    for(int i=0; i<image.channels(); i++)
        cv::equalizeHist(splitBGR[i],splitBGR[i]);
    cv::merge(splitBGR,mergeImg);
    cv::Mat blur_image = cv::Mat::zeros( image.size(), image.type() );
    cv::blur(new_image,blur_image,cv::Size( 8, 8), cv::Point(-1,-1),cv::BORDER_DEFAULT);
    cv::Mat src=image;
    cv::Mat res,roi,reverse_roi,reverse_res;
    res=cv::Mat::zeros( image.size(), image.type() );
    reverse_res=cv::Mat::zeros( image.size(), image.type() );
    roi=cv::Mat::zeros( image.size(), image.type());
    reverse_roi=cv::Mat(image.size(),image.type());
    for( int y = 0; y < reverse_roi.rows; y++ )
    {
        for( int x = 0; x < reverse_roi.cols; x++ )
        {
                reverse_roi.at<cv::Vec3b>(y,x)[0] = 255;
                reverse_roi.at<cv::Vec3b>(y,x)[1] = 255;
                reverse_roi.at<cv::Vec3b>(y,x)[2] = 255;
        }
    }
    int thickness = 2;
    int lineType = 8;
    double angle=90;
    double w=image.rows;
    cv::ellipse( roi,

           cv::Point( image.cols/2, image.rows/2 ),

           cv::Size( image.cols/3.0, image.rows/2.0 ),

           angle,

           0,

           360,

           cv::Scalar( 255, 255, 255 ),

           thickness,

           lineType );

    cv::ellipse(reverse_roi, cv::Point( image.cols/2, image.rows/2 ), cv::Size( image.cols/4.0, image.rows/2.0 ), angle,0,360,cv::Scalar( 0, 0, 0 ),thickness,lineType );
    cv::floodFill(reverse_roi, cv::Point(w/2, w/2), cv::Scalar( 0, 0, 0 ));
    cv::floodFill(roi, cv::Point(w/2, w/2), cv::Scalar( 255, 255, 255 ));
    image.copyTo(res,roi);
    image.copyTo(reverse_res,reverse_roi);
    cv::cvtColor(res,res,CV_BGR2HSV);
    for( int y = 0; y < res.rows; y++ )
    {
        for( int x = 0; x < res.cols; x++ )
        {
                res.at<cv::Vec3b>(y,x)[1] = res.at<cv::Vec3b>(y,x)[1];
                res.at<cv::Vec3b>(y,x)[2] = res.at<cv::Vec3b>(y,x)[2];
        }
    }
    cv::cvtColor(res,res,CV_HSV2BGR);
    cv::cvtColor(reverse_res,reverse_res,CV_BGR2HSV);
    for( int y = 0; y < reverse_res.rows; y++ )
    {
        for( int x = 0; x < reverse_res.cols; x++ )
        {
                reverse_res.at<cv::Vec3b>(y,x)[1] = 1.5*reverse_res.at<cv::Vec3b>(y,x)[1];
                if(reverse_res.at<cv::Vec3b>(y,x)[1]>255)
                {
                    reverse_res.at<cv::Vec3b>(y,x)[1]=255;
                }
                reverse_res.at<cv::Vec3b>(y,x)[2] = reverse_res.at<cv::Vec3b>(y,x)[2];
        }
    }
    cv::cvtColor(reverse_res,reverse_res,CV_HSV2BGR);
    reverse_res=reverse_res+res;
    for( int y = 0; y < image.rows; y++ )
    {
        for( int x = 0; x < image.cols; x++ )
        {

                reverse_res.at<cv::Vec3b>(y,x)[0] =  10*sqrt(reverse_res.at<cv::Vec3b>(y,x)[0]);
                reverse_res.at<cv::Vec3b>(y,x)[1] = reverse_res.at<cv::Vec3b>(y,x)[1];
                reverse_res.at<cv::Vec3b>(y,x)[2] =  reverse_res.at<cv::Vec3b>(y,x)[2];
        }
    }
    cv::blur(reverse_res,reverse_res,cv::Size( 2, 2), cv::Point(-1,-1),cv::BORDER_DEFAULT);
    dst = reverse_res.clone();
}

void effect::_blur(const Mat &src, Mat &dst, uint w, uint h)
{
    blur(src, dst, Size(w, h));
}

void effect::_gaussianBlur(const Mat &src, Mat &dst, uint w, uint h)
{
    GaussianBlur(src, dst, Size(w, h), 0);
}

void effect::_medianBlur(const Mat &src, Mat &dst, uint ksize)
{
    medianBlur(src, dst, ksize);
}

void effect::_bilateralFilter(const Mat &src, Mat &dst, double sigmaColor, double sigmaSpace)
{
        bilateralFilter(src, dst, 0, sigmaColor, sigmaSpace);
}
