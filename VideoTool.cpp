#include <sstream>
#include <string>
#include <iostream>
#include <cmath>
#include <ctime>
#include <cstdlib>
#include <unistd.h>

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

//#include <opencv2\highgui.h>
#include "opencv2/highgui/highgui.hpp"
//#include <opencv2\cv.h>
#include "opencv2/opencv.hpp"

#define sec 1000000

using namespace std;
using namespace cv;

char ip[50] = {"193.226.12.217"};
const char *moveForward = "F";
const char *moveBack = "B";
const char *moveRight = "R";
const char *moveLeft = "L";
const char *stop = "S";
int port = 20236, goodMove;
double oldDis;

struct robot
{
	int x, y;
};

robot robotRoz, robotGalben;

//initial min and max HSV filter values.
//these will be changed using trackbars
int H_MIN = 164;
int H_MAX = 181;
int S_MIN = 49;
int S_MAX = 256;
int V_MIN = 0;
int V_MAX = 256;

int H_MIN1 = 28;
int H_MAX1 = 56;
int S_MIN1 = 49;
int S_MAX1 = 256;
int V_MIN1 = 0;
int V_MAX1 = 256;
//default capture width and height
const int FRAME_WIDTH = 640;
const int FRAME_HEIGHT = 480;
//max number of objects to be detected in frame
const int MAX_NUM_OBJECTS = 50;
//minimum and maximum object area
const int MIN_OBJECT_AREA = 20 * 20;
const int MAX_OBJECT_AREA = FRAME_HEIGHT * FRAME_WIDTH / 1.5;
//names that will appear at the top of each window
const std::string windowName = "Original Image";
const std::string windowName1 = "HSV Image";
const std::string windowName2 = "Thresholded Image";
const std::string windowName3 = "After Morphological Operations";
const std::string windowName4 = "HSV Image2";
const std::string trackbarWindowName = "Trackbars";

void on_mouse(int e, int x, int y, int d, void *ptr)
{
	if (e == EVENT_LBUTTONDOWN)
	{
		cout << "Left button of the mouse is clicked - position (" << x << ", " << y << ")" << endl;
	}
}

void on_trackbar(int, void *)
{ //This function gets called whenever a
	// trackbar position is changed
}

string intToString(int number)
{

	std::stringstream ss;
	ss << number;
	return ss.str();
}

void createTrackbars()
{
	//create window for trackbars

	namedWindow(trackbarWindowName, 0);
	//create memory to store trackbar name on window
	char TrackbarName[50];
	sprintf(TrackbarName, "H_MIN", H_MIN);
	sprintf(TrackbarName, "H_MAX", H_MAX);
	sprintf(TrackbarName, "S_MIN", S_MIN);
	sprintf(TrackbarName, "S_MAX", S_MAX);
	sprintf(TrackbarName, "V_MIN", V_MIN);
	sprintf(TrackbarName, "V_MAX", V_MAX);
	//create trackbars and insert them into window
	//3 parameters are: the address of the variable that is changing when the trackbar is moved(eg.H_LOW),
	//the max value the trackbar can move (eg. H_HIGH),
	//and the function that is called whenever the trackbar is moved(eg. on_trackbar)
	//                                  ---->    ---->     ---->
	createTrackbar("H_MIN", trackbarWindowName, &H_MIN, H_MAX, on_trackbar);
	createTrackbar("H_MAX", trackbarWindowName, &H_MAX, H_MAX, on_trackbar);
	createTrackbar("S_MIN", trackbarWindowName, &S_MIN, S_MAX, on_trackbar);
	createTrackbar("S_MAX", trackbarWindowName, &S_MAX, S_MAX, on_trackbar);
	createTrackbar("V_MIN", trackbarWindowName, &V_MIN, V_MAX, on_trackbar);
	createTrackbar("V_MAX", trackbarWindowName, &V_MAX, V_MAX, on_trackbar);
}
void drawObject(int x, int y, Mat &frame)
{

	//use some of the openCV drawing functions to draw crosshairs
	//on your tracked image!

	//UPDATE:JUNE 18TH, 2013
	//added 'if' and 'else' statements to prevent
	//memory errors from writing off the screen (ie. (-25,-25) is not within the window!)

	circle(frame, Point(x, y), 20, Scalar(0, 255, 0), 2);
	if (y - 25 > 0)
		line(frame, Point(x, y), Point(x, y - 25), Scalar(0, 255, 0), 2);
	else
		line(frame, Point(x, y), Point(x, 0), Scalar(0, 255, 0), 2);
	if (y + 25 < FRAME_HEIGHT)
		line(frame, Point(x, y), Point(x, y + 25), Scalar(0, 255, 0), 2);
	else
		line(frame, Point(x, y), Point(x, FRAME_HEIGHT), Scalar(0, 255, 0), 2);
	if (x - 25 > 0)
		line(frame, Point(x, y), Point(x - 25, y), Scalar(0, 255, 0), 2);
	else
		line(frame, Point(x, y), Point(0, y), Scalar(0, 255, 0), 2);
	if (x + 25 < FRAME_WIDTH)
		line(frame, Point(x, y), Point(x + 25, y), Scalar(0, 255, 0), 2);
	else
		line(frame, Point(x, y), Point(FRAME_WIDTH, y), Scalar(0, 255, 0), 2);

	putText(frame, intToString(x) + "," + intToString(y), Point(x, y + 30), 1, 1, Scalar(0, 255, 0), 2);
	//cout << "x,y: " << x << ", " << y;
}
void morphOps(Mat &thresh)
{

	//create structuring element that will be used to "dilate" and "erode" image.
	//the element chosen here is a 3px by 3px rectangle

	Mat erodeElement = getStructuringElement(MORPH_RECT, Size(3, 3));
	//dilate with larger element so make sure object is nicely visible
	Mat dilateElement = getStructuringElement(MORPH_RECT, Size(8, 8));

	erode(thresh, thresh, erodeElement);
	erode(thresh, thresh, erodeElement);

	dilate(thresh, thresh, dilateElement);
	dilate(thresh, thresh, dilateElement);
}
void trackFilteredObject(int &x, int &y, Mat threshold, Mat &cameraFeed)
{

	Mat temp;
	threshold.copyTo(temp);
	//these two vectors needed for output of findContours
	vector<vector<Point> > contours;
	vector<Vec4i> hierarchy;
	//find contours of filtered image using openCV findContours function
	findContours(temp, contours, hierarchy, CV_RETR_CCOMP, CV_CHAIN_APPROX_SIMPLE);
	//use moments method to find our filtered object
	double refArea = 0;
	bool objectFound = false;
	if (hierarchy.size() > 0)
	{
		int numObjects = hierarchy.size();
		//if number of objects greater than MAX_NUM_OBJECTS we have a noisy filter
		if (numObjects < MAX_NUM_OBJECTS)
		{
			for (int index = 0; index >= 0; index = hierarchy[index][0])
			{

				Moments moment = moments((cv::Mat)contours[index]);
				double area = moment.m00;

				//if the area is less than 20 px by 20px then it is probably just noise
				//if the area is the same as the 3/2 of the image size, probably just a bad filter
				//we only want the object with the largest area so we safe a reference area each
				//iteration and compare it to the area in the next iteration.
				if (area > MIN_OBJECT_AREA && area < MAX_OBJECT_AREA && area > refArea)
				{
					x = moment.m10 / area;
					y = moment.m01 / area;
					objectFound = true;
					refArea = area;
				}
				else
					objectFound = false;
			}
			//let user know you found an object
			if (objectFound == true)
			{
				putText(cameraFeed, "Tracking Object", Point(0, 50), 2, 1, Scalar(0, 255, 0), 2);
				//draw object location on screen
				//cout << x << "," << y;
				drawObject(x, y, cameraFeed);
			}
		}
		else
			putText(cameraFeed, "TOO MUCH NOISE! ADJUST FILTER", Point(0, 50), 1, 2, Scalar(0, 0, 255), 2);
	}
}

int randomGen()
{
	int randomNr = rand() % 4;
	return randomNr;
}

int connectToRobot()
{
	int sockfd, portno;
	struct sockaddr_in serv_addr;
	struct hostent *server;

	portno = port;
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
	{
		cout << "ERROR opening socket!\n";
		exit(0);
	}
	server = gethostbyname(ip);
	if (server == NULL)
	{
		cout << "ERROR no such host\n";
		exit(0);
	}
	bzero((char *)&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
	serv_addr.sin_port = htons(portno);
	if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		cout << "ERROR connecting!\n";
		exit(0);
	}
	return sockfd;
}

void makeMove(int sockfd,const char *move)
{
	int n;
	n = send(sockfd, move, strlen(move), 0);
	if (n < 0)
	{
		cout << "ERROR writing to socket!\n";
		exit(0);
	}
}

void battleAlgorithm(int sockfd)
{ 
	double newDis = sqrt(pow((robotGalben.x - robotRoz.x), 2) + pow((robotGalben.y - robotRoz.y), 2));	
	int nr=0;
	if(newDis < oldDis){
		switch(goodMove){
			case 0:
						makeMove(sockfd, moveForward);
						usleep(sec);
						makeMove(sockfd, stop);
						break;
			case 1:
						makeMove(sockfd, moveBack);
						usleep(sec);
						makeMove(sockfd, stop);
						break;
			case 2:
						makeMove(sockfd, moveRight);
						usleep(sec);
						makeMove(sockfd, stop);
						break;
			case 3:	
						makeMove(sockfd, moveLeft);
						usleep(sec);
						makeMove(sockfd, stop);
						break;
		}	
	}else{
		while(nr == goodMove){
			nr = randomGen();	
		}
		switch(nr){
				case 0:
							makeMove(sockfd, moveForward);
							usleep(sec);
							makeMove(sockfd, stop);
							goodMove = 0;
				break;
				case 1:
							makeMove(sockfd, moveBack);
							usleep(sec);
							makeMove(sockfd, stop);
							goodMove = 1;
				break;
				case 2:
							makeMove(sockfd, moveRight);
							usleep(sec);
							makeMove(sockfd, stop);
							goodMove = 2;
				break;
				case 3:	
							makeMove(sockfd, moveLeft);
							usleep(sec);
							makeMove(sockfd, stop);
							goodMove = 3;
				break;
		}
	}
	oldDis = newDis;	
}

int main(int argc, char *argv[])
{
	srand(time(0));	
	int sockfd, i=0;
	//some boolean variables for different functionality within this
	//program
	bool trackObjects = true;
	bool useMorphOps = true;

	Point p;
	//Matrix to store each frame of the webcam feed
	Mat cameraFeed;
	//matrix storage for HSV image
	Mat HSV;
	//matrix storage for binary threshold image
	Mat threshold, threshold1;
	//create slider bars for HSV filtering
	createTrackbars();
	//video capture object to acquire webcam feed
	VideoCapture capture;
	//open capture object at location zero (default location for webcam)
	capture.open("rtmp://172.16.254.99/live/nimic");
	//set height and width of capture frame
	capture.set(CV_CAP_PROP_FRAME_WIDTH, FRAME_WIDTH);
	capture.set(CV_CAP_PROP_FRAME_HEIGHT, FRAME_HEIGHT);
	//start an infinite loop where webcam feed is copied to cameraFeed matrix
	//all of our operations will be performed within this loop
	sockfd = connectToRobot();
	while (1)
	{
		//store image to matrix
		capture.read(cameraFeed);
		//convert frame from BGR to HSV colorspace
		cvtColor(cameraFeed, HSV, COLOR_BGR2HSV);
		//filter HSV image between values and store filtered image to
		//threshold matrix
		inRange(HSV, Scalar(H_MIN, S_MIN, V_MIN), Scalar(H_MAX, S_MAX, V_MAX), threshold);
		inRange(HSV, Scalar(H_MIN1, S_MIN1, V_MIN1), Scalar(H_MAX1, S_MAX1, V_MAX1), threshold1);
		//perform morphological operations on thresholded image to eliminate noise
		//and emphasize the filtered object(s)
		if (useMorphOps)
		{
			morphOps(threshold);
			morphOps(threshold1);
		}
		//pass in thresholded frame to our object tracking function
		//this function will return the x and y coordinates of the
		//filtered object
		if (trackObjects)
		{
			trackFilteredObject(robotRoz.x, robotRoz.y, threshold, cameraFeed);
			trackFilteredObject(robotGalben.x, robotGalben.y, threshold1, cameraFeed);
		}
		//show frames
		imshow(windowName2, threshold);
		imshow(windowName4, threshold1);
		imshow(windowName, cameraFeed);
		//imshow(windowName1, HSV);
		setMouseCallback("Original Image", on_mouse, &p);
		if(i == 0){
			oldDis = sqrt(pow((robotGalben.x - robotRoz.x), 2) + pow((robotGalben.y - robotRoz.y), 2));
			makeMove(sockfd, moveForward);
			usleep(sec);
			makeMove(sockfd, stop);
		}else{
			battleAlgorithm(sockfd);	
		}
		i++;
		//delay 30ms so that screen can refresh.
		//image will not appear without this waitKey() command
		waitKey(30);
	}
	return 0;
}
