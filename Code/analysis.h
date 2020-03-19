#pragma once
#ifndef ANALYSIS_H
#define ANALYSIS_H
#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <cmath>
#include "veinfeature.h"
#include "globals.h"

#define WHITE 65535
#define BLACK 0

struct Coordinate
{
	int x, y;
};

void getVeinPattern(cv::Mat ir, cv::Mat depth, unsigned short minDepth, unsigned short maxDepth, struct package * result, int mode);
long double analysePatterns(cv::Mat& ref, cv::Mat& test);
static vector<Coordinate> sequence_points(cv::Mat& trimmed);
static double euclidean_distance(Coordinate c1, Coordinate c2);
static long double calculate_similarity(vector<Coordinate> ref, vector<Coordinate> test);
static double KF(Coordinate first, Coordinate second, double kernel);

#endif /*ANALYSIS_H*/