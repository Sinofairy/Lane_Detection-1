#include"main.hpp"
bool debug_main = false;


int main(int argc, char* argv[])
{

	Mat input_image = imread("/home/nvidia/training/image_2/um_000004.png");

	unsigned char* h_rgb_img = input_image.data;

	/*Gererate IPM View*/

	float bev_res = 0.1;
	tuple_int bev_xRange_minMax = {-10,10};
	tuple_int bev_zRange_minMax = {6, 46};
	float invalid_value = -numeric_limits<float>::infinity(); 

	BirdsEyeView bev(bev_res, invalid_value,bev_xRange_minMax, bev_zRange_minMax);
	/*Projection matrix for left color camera in rectified coordinates*/
	matrix_t intrinsic_matrix
	{
		{7.215377000000e+02, 0.000000000000e+00, 6.095593000000e+02, 4.485728000000e+01},
		{0.000000000000e+00 ,7.215377000000e+02 ,1.728540000000e+02 ,2.163791000000e-01},
		{0.000000000000e+00, 0.000000000000e+00, 1.000000000000e+00, 2.745884000000e-03}
	};
	
	/*Initialize Rotation Matrix (3x3) */
	matrix_t rotation_matrix
	{
		{9.999239000000e-01, 9.837760000000e-03, -7.445048000000e-03},
		{-9.869795000000e-03, 9.999421000000e-01,-4.278459000000e-03},
		{7.402527000000e-03, 4.351614000000e-03, 9.999631000000e-01}
	};
	
	matrix_t Tr_cam_to_road = readcalibfile("/home/nvidia/training/calib/um_000004.txt");
	//print2dvector(Tr_cam_to_road);

	bev.setup(intrinsic_matrix, rotation_matrix, Tr_cam_to_road);
	bev.initialize();

	//auto begin = std::chrono::high_resolution_clock::now();
	unsigned char* h_grayImage =  rgb2gray(h_rgb_img);


	Mat gray_image(IMAGE_HEIGHT_RGB, IMAGE_WIDTH_RGB, CV_8UC1);
	unsigned char* img = gray_image.data;

	auto begin = std::chrono::high_resolution_clock::now();

	unsigned char* ipm_image = bev.computeLookUpTable(h_grayImage);
	
	Mat gray_IPM_image(IMAGE_HEIGHT, IMAGE_WIDTH, CV_8UC1);
	unsigned char* o_im  = gray_IPM_image.data;
	for(int i =0 ;i< IMAGE_HEIGHT;i++)
	{
		for(int j =0;j<IMAGE_WIDTH;j++)
		{
			gray_IPM_image.at<unsigned char>(i,j) = *(ipm_image + i*IMAGE_WIDTH + j);
			//cout<<(int)*(ipm_image + i*IMAGE_WIDTH + j)<<"\t";
		}
	}

	
	if(debug_main)
	{
		imshow("Result", gray_IPM_image);
		waitKey(0);
		imwrite("/home/nvidia/IPM_test_image_10.png", gray_IPM_image);
	}

	unsigned char* bin_image = convert2fp(ipm_image);

	if(debug_main)
	{
		Mat output_image(ROI_IMAGE_HEIGHT, ROI_IMAGE_WIDTH, CV_8UC1);
		unsigned char* poutputimage = output_image.data;
		for(int i  =0;i<ROI_IMAGE_HEIGHT;i++)
		{
			for(int j  =0;j<ROI_IMAGE_WIDTH;j++)
			{
				*(poutputimage + i*ROI_IMAGE_WIDTH + j) = *(bin_image + i*ROI_IMAGE_WIDTH +j);

			}
		}
	
		imshow("Result", output_image);
		waitKey(0);
	}

	float rMin = 0;
	float rMax = (IMG_WIDTH + IMG_HEIGHT)*2 + 1;
	float rStep = 1.0;

	float thetaMax = 180;
	float thetaMin = 0;
	float thetaStep = 1;

	const int numangle = std::round((thetaMax - thetaMin)/thetaStep);
	const int numrho = std::round(rMax/rStep);

	lines_w_non_zero* values = houghTransform(bin_image, numangle, numrho, thetaStep, rStep);

	int line_count = values->hough_lines->countlines;
	//cout<<"Line Count \t"<<line_count<<endl;

	if(debug_main)
	{
		for(int i  =0;i<line_count;i++)
		{
			//float theta_line = (hough_lines->lines + i)->y;
			//float rho = (hough_lines->lines + i)->x;
	
			float theta_line = (values->hough_lines->lines + i)->y;
			float rho = (values->hough_lines->lines + i)->x;
			double a = cos(theta_line);
			double b = sin(theta_line);

			double x0 = a*rho;
			double y0 = b*rho;

			cv::Point pt1, pt2;
			pt1.x = (int)(x0 + 400*(-b));
			pt1.y = (int)(y0 + 400*(a));
			pt2.x = (int)(x0 - 400*(-b));
			pt2.y = (int)(x0 - 400*(a));

			line(gray_IPM_image, pt1,pt2, (255,0,0),1);
		}
		imshow("Inital Guess After Hough", gray_IPM_image);
		waitKey(0);
	}

	vector<Line> line_objects(line_count);
	getLineObjects(line_objects, values->hough_lines,values->votes,IMAGE_WIDTH, IMAGE_HEIGHT);
	
	vector<Linepoint> x_y_points = initializePoints(line_objects, values->clist, values->count);	

	if(debug_main)
	{
		for(int i =0;i<line_objects.size();i++)
		{
			for(int j = 0;j<line_objects[i].x_y_points.size();j++)
			{
				cout<<"X Coordinate \t"<<line_objects[i].x_y_points[j].x<<"\t"<<"Y Coordinate \t"<<line_objects[i].x_y_points[j].y<<"\t";

			}

			cout<<endl;
		}
	
		cout<<"Number of Lines \t"<<line_objects.size()<<endl;

		for(int i =0;i<line_objects.size();i++)
		{
			cout<<"startpoint \t"<<line_objects[i].startpoint.x<<"\t"<<line_objects[i].startpoint.y<<endl;
			cout<<"endpoint \t"<<line_objects[i].endpoint.x<<"\t"<<line_objects[i].endpoint.y<<endl;
		}
	}

	for(int i =0; i<line_objects.size();i++)
	{	
		//fit_line(line_objects[i], gray_IPM_image);
		getPolyFit(line_objects[i], gray_IPM_image, x_y_points);	

	}

	unsigned char* line_detected_image = gray_IPM_image.data;
	unsigned char* perspective_image = bev.getperspectiveView(line_detected_image);


	Mat gray_IPM_image_detected(IMAGE_HEIGHT_RGB, IMAGE_WIDTH_RGB, CV_8UC1);
	unsigned char* p_im_pointer = gray_IPM_image_detected.data;

	for(int i =0;i<IMAGE_HEIGHT_RGB;i++)
	{
		for(int j =0;j<IMAGE_WIDTH_RGB;j++)
		{
		
			*(p_im_pointer + i*IMAGE_WIDTH_RGB + j) = *(perspective_image + i*IMAGE_WIDTH_RGB + j);

		}
	}

	if(debug_main)
	{
		imshow("Result", gray_IPM_image_detected);
		waitKey(0);
		imwrite("/home/nvidia/Lane_Detected_Images_Perspective/lane_image_persp_12.png", gray_IPM_image_detected);
	}

	auto end = std::chrono::high_resolution_clock::now();
	cout << std::chrono::duration_cast<std::chrono::nanoseconds>(end-begin).count() << "ns" << std::endl;
	
	imshow("Result", gray_IPM_image);
	waitKey(0);


	//imwrite("/home/nvidia/Lane_Detected_Images/lane_image_ipm_12.png", gray_IPM_image);



}
