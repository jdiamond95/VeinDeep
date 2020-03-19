#pragma once
#ifndef GLOBALS_H
#define GLOBALS_H
#include <opencv2\core\core.hpp>
#include <opencv2\highgui\highgui.hpp>
struct package {
	cv::Mat silhouette, vein_pattern;
};

#define DECREASED 0
#define INCREASED 1
#define NORMAL 2
#endif /*GLOBALS_H */