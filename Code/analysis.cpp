#include "analysis.h"

using namespace cv;
using namespace std;

void getVeinPattern(cv::Mat ir, cv::Mat depth, unsigned short minDepth, unsigned short maxDepth, struct package * result, int mode)
{
	// Read the input images
	//Mat ref_ir = imread("my_ir_image.png", CV_LOAD_IMAGE_ANYCOLOR | CV_LOAD_IMAGE_ANYDEPTH);
	//Mat ref_depth = imread("my_depth_image.png", CV_LOAD_IMAGE_ANYCOLOR | CV_LOAD_IMAGE_ANYDEPTH);
	Mat ref_ir = ir;
	Mat ref_depth = depth;

	// ROI
	int x1 = 230;// 200;
	int x2 = 370;// 400;
	int y1 = 210;// 180;
	int y2 = 370;// 400;

	// Distance threshold parameters
	unsigned short min_depth = minDepth;// 500;
	unsigned short max_depth = maxDepth;// 600;
	// Silhouette scale parameters
	double scale_x = 0.8;// 1;
	double scale_y = 0.8;//1;
	// Vein kernel size parameter
	int kernel_size = 19;
	// Remove vein segments smaller than the following
	int min_vein_size = 20;//20 
	// Segment variables
	int connected = 4;//8

	// Initialise class
	veinfeature feature = veinfeature(x1, x2, y1, y2, max_depth, min_depth, scale_x, scale_y, kernel_size, min_vein_size, connected);

	// Compute the vein patterns and save the image	
	feature.get_vein_pattern(ref_depth, ref_ir, result, mode);	
}

long double analysePatterns(cv::Mat& ref, cv::Mat& test) {

	//printf("ref.size() = %d\n", ref.size());

	vector<Coordinate> S, T;
	S = sequence_points(ref);
	T = sequence_points(test);

	long double score = calculate_similarity(S, T);
	printf("similarity score = %lf | \n", score);
	// logic for similarity score interpretation	
	return score;
}

vector<Coordinate> sequence_points(cv::Mat& trimmed) {
	vector<Coordinate> coordinates;
	int numCoordsCreated = 0;
	for (int numRow = 0; numRow < trimmed.rows; numRow++) {
		for (int numColumn = 0; numColumn < trimmed.cols; numColumn++) {
			if (trimmed.at<unsigned short>(numRow, numColumn) == WHITE) {
				Coordinate temp;
				temp.x = numColumn;
				temp.y = numRow;
				coordinates.push_back(temp);
				numCoordsCreated++;
			}
		}
	}
	return coordinates;
}

long double calculate_similarity(vector<Coordinate> ref, vector<Coordinate> test) {
	long double similarity_score = 0;
	long double score_1 = 0;
	long double score_2 = 0;
	long double score_3 = 0;
	double kernelSize = 5;

	for (Coordinate s: ref) {
		for (Coordinate sDash: ref) {
			score_1 += KF(s, sDash, kernelSize);	
		}
	}
	for (Coordinate t: test) {
		for (Coordinate tDash: test) {
			score_2 += KF(t, tDash, kernelSize);
		}
	}
	for (Coordinate s : ref) {
		for (Coordinate t : test) {		
			score_3 += KF(s, t, kernelSize);
		}
	}
	printf("s_1 = %lf, s_2 = %lf, s_3 = %lf | ", score_1, score_2, score_3);
	similarity_score = ((score_1 + score_2) - 2 * score_3);
	return similarity_score;
}

// the gaussian function
double KF(Coordinate first, Coordinate second, double kernel) {	
	double euc_dist = euclidean_distance(first, second);
	double eucSquared = pow(euc_dist, 2);
	double kernelSquared = pow(kernel, 2);
	double result = exp((0-eucSquared)/(kernelSquared));
	return result;
}

double euclidean_distance(Coordinate c1, Coordinate c2) {
	double distance;
	distance = sqrt(pow((c1.x - c2.x), 2) + pow((c1.y - c2.y), 2));
	return distance;
}