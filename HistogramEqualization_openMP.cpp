// Histogram.cpp : 此檔案包含 'main' 函式。程式會於該處開始執行及結束執行。
//

#include <Windows.h>
#include <fstream>
#include <iostream>
#include <string>
#include <ios>
#include <omp.h>

typedef struct
{
	uint8_t Grey;
} strct_Greyscale;

typedef struct
{
	bool type;
	uint32_t size;
	uint32_t height;
	uint32_t weight;
	uint8_t depth;
	uint8_t header[54];
	uint8_t* data;
} Image;

Image* readbmp(const char* filename)
{
	std::ifstream bmp(filename, std::ios::binary);
	if (bmp.fail())
	{
		printf("Can't open file %s,exit 1", filename);
		exit(1);
	}
	char header[54];
	bmp.read(header, 54);
	uint32_t size = *(int*)& header[2];
	uint32_t offset = *(int*)& header[10];
	uint32_t w = *(int*)& header[18];
	uint32_t h = *(int*)& header[22];
	uint16_t depth = *(uint16_t*)& header[28];

	if (depth != 8 && depth != 24 && depth != 32)
	{
		printf("Not support depth %d\n ", depth);
		exit(1);
	}
	bmp.seekg(offset, bmp.beg);

	Image* ret = new Image();
	ret->type = 1;
	ret->height = h;
	ret->weight = w;
	ret->size = w * h;
	ret->depth = depth;
	memcpy(ret->header, header, 54);

	ret->data = new uint8_t[w * h * (depth / 8)];

	bmp.read((char*)(ret->data ), ret->size *(depth / 8) );



	return ret;
}

int writebmp(const char* filename, Image* img)
{
	std::ofstream fout;
	fout.open(filename, std::ios::binary);

	
	uint8_t header[54] = {
	0x42,        // identity : B
	0x4d,        // identity : M
	0, 0, 0, 0,  // file size
	0, 0,        // reserved1
	0, 0,        // reserved2
	54, 0, 0, 0, // bmp data offset
	40, 0, 0, 0, // struct BITMAPINFOHEADER size
	0, 0, 0, 0,  // bmp width
	0, 0, 0, 0,  // bmp height
	1, 0,        // planes
	0, 0,       // bit per pixel
	0, 0, 0, 0,  // compression
	0, 0, 0, 0,  // data size
	0, 0, 0, 0,  // h resolution
	0, 0, 0, 0,  // v resolution
	0, 0, 0, 0,  // used colors
	0, 0, 0, 0   // important colors
	};

	// file size
	uint32_t file_size = img->size * (img->depth/8) + 54;	

	// 8bit color table
	if (img->depth == 8) {
		file_size += 1024;
		*((uint32_t*)& header[10]) = 54 + 1024;
	}

	memcpy(&header[2], (uint8_t*)& file_size, 4);
	
	// depth
	header[0x1c] = img->depth;

	// width
	uint32_t width = img->weight;
	memcpy(&header[18], (uint8_t*)& width, 4);	

	// height
	uint32_t height = img->height;
	memcpy(&header[22], (uint8_t*)& height, 4);


	// Data size
	uint32_t data_size = img->size * (img->depth / 8) ;
	memcpy(&header[34], (uint8_t*)& data_size, 4);

	//fout.write((char*)img->header, 54);
	fout.write((char*)header, 54);

	//color table for 8bit case
	if (img->depth == 8) {
		uint32_t colortable[256];
		for (uint32_t i = 0; i < 256; i++) {
			colortable[i] = (i << 16) + (i << 8) + i;
		}
		fout.write((char*)colortable, 256 * 4);
	}
	//fout.write((char *)img->data, img->size * 4);
	fout.write((char*)img->data, img->size*(img->depth/8));
	fout.close();
	return 0;				// dummy return;
}
/*
void histogram(Image* img, uint32_t GreyScaleHistogram[256]) {
	std::fill(GreyScaleHistogram, GreyScaleHistogram + 256, 0);

	for (int i = 0; i < img->size; i++) {
		uint8_t pixelGrey = img->data[i*(img->depth/8)];
		GreyScaleHistogram[pixelGrey]++;
	}
}
*/

int outputHistogramData(const char* filename, uint32_t histogram[256])
{
	std::ofstream fout;
	fout.open(filename, std::ofstream::out );
	char strData[10];
	
	for (int i = 0; i < 256; i++) {		
		sprintf_s(strData,"%d\n", histogram[i]);
		std::string strOut = strData;
		fout.write(strOut.c_str(), strlen(strOut.c_str()));
	}

	
	fout.close();
	return 0;				// dummy return;
}



int main(int argc, char* argv[])
{
	LARGE_INTEGER m_liPerfStart_NoFileIo = { 0 };
	LARGE_INTEGER liPerfNow_NoFileIo = { 0 };
	uint32_t GreyScaleHistogram[256];	
	LARGE_INTEGER m_liPerfFreq = { 0 };		
	LARGE_INTEGER m_liPerfStart = { 0 };
	int chk[200][1];
	// 取得目前 CPU frequency
	QueryPerformanceFrequency(&m_liPerfFreq);

	// 取得執行前時間
	QueryPerformanceCounter(&m_liPerfStart);
	

	char* filename;
	if (argc >= 2)
	{		
		int LUT[256];
		uint32_t NumberOfPixel;
		uint32_t sum;
		uint8_t* dataOfDst;
		Image* Transfer_img;
		uint8_t* dataOfSrc;
		uint32_t NumberOfData;
		int NumThreads;
		char* str_NumThreads;
		bool bFillZero = false;

		str_NumThreads = argv[1];
		NumThreads = atoi(str_NumThreads);
		if (NumThreads < 0 || NumThreads>4) {
			printf("invalid parameter of thread number:%d", NumThreads);
			exit(1);
		}

		// 取得檔名
		filename = argv[2];

		// 讀檔，取得header和data
		Image* img = readbmp(filename);


		// 取得執行時間(不包含讀檔寫檔)
		QueryPerformanceCounter(&m_liPerfStart_NoFileIo);
	
		//	omp_set_num_threads(img->data[0]&3+1);
		omp_set_num_threads(NumThreads);
		#pragma omp parallel
		{
			/*------------------------------------
			  各灰階度統計
			------------------------------------*/
			// 初始 0
			// 因每個iterations工作量相同，使用static,chunk_size=iter_cnt/thread_cnt
			// 應可讓每個thread工作負載相同
			#pragma omp  for schedule(static)
			for (int i = 0; i < 256; i++) {
				GreyScaleHistogram[i] = 0;
			}
			// 讀取像素點灰階值，累加GreyScaleHistogram陣列
			// 對應index裡的element
			// 因每個iterations工作量相同，使用static,chunk_size=iter_cnt/thread_cnt
			// 應可讓每個thread工作負載相同
			#pragma omp  for schedule(static)
			for (int i = 0; i < img->size; i++) {
				uint8_t pixelGrey = img->data[i * (img->depth / 8)];
				GreyScaleHistogram[pixelGrey]++;
			}

			/*------------------------------------
			  直方圖均衡化
			------------------------------------*/
			#pragma omp single
			{
				NumberOfPixel = img->size;
				LUT[0] = 1.0 * GreyScaleHistogram[0] / NumberOfPixel * 255;
				sum = GreyScaleHistogram[0];
			
				// 此for loop可改為
				//  #serial
				//  for i:=1 to 255
				//     sum[i] += GreyScaleHistogram[i]
				//
				//  #parallel
				//  for i := 1 to 255
				//    LUT[i] =  1.0 * sum[i] / NumberOfPixel * 255;
				//  但考量到計算量小和平行化後的overhead，還是只用單線程處理
				for (int i = 1; i <= 255; ++i){
					sum += GreyScaleHistogram[i];
					LUT[i] = 1.0 * sum / NumberOfPixel * 255;
				}
			
				dataOfSrc = img->data;
				dataOfDst = new uint8_t[img->size * (img->depth / 8)];
				NumberOfData = NumberOfPixel * (img->depth / 8);

				if (img->depth == 32)
					bFillZero = true;
			}

			// 因每4個iterations工作量相同，使用static,chunk_size=iter_cnt/thread_cnt
			// 應可讓每個thread工作負載相同
			#pragma omp  for schedule(static)
			for (uint32_t i = 0; i < NumberOfData; ++i)
			{
				if (bFillZero) {
					if ((i % 4) != 3) {
						dataOfDst[i] = LUT[dataOfSrc[i]];
					}
					else {
						dataOfDst[i] = 0;
					}
				}
				else {
					dataOfDst[i] = LUT[dataOfSrc[i]];
				}
			}


		}

		Transfer_img = new Image();
		Transfer_img->size = img->size;
		Transfer_img->weight = img->weight;
		Transfer_img->height = img->height;
		Transfer_img->depth = img->depth;
		Transfer_img->data = dataOfDst;
		memcpy(Transfer_img->header, img->header, 54);


		// 取得執行後時間(不包含讀檔寫檔)		
		QueryPerformanceCounter(&liPerfNow_NoFileIo);

		std::string Transferfile = "Transfer_" + std::string(filename);
		writebmp(Transferfile.c_str(), Transfer_img);
		

		// 輸出前後histogram數據，供demo 和 report使用
		std::string origin = "OriginHistogram_" + std::string(filename) + ".txt";
		outputHistogramData(origin.c_str(), GreyScaleHistogram);
		uint32_t TransHistogram[256] = {0};
		for (int i = 0; i < img->size; i++) {
			uint8_t pixelGrey = dataOfDst[i * (img->depth / 8)];
			TransHistogram[pixelGrey]++;
		}
		std::string trans = "TransHistogram_" + std::string(filename) +".txt";
		outputHistogramData(trans.c_str(), TransHistogram);
	}
	else {
		printf("Usage: ./app_name.exe <numOfThread> <img.bmp>\n");
	}

	// 取得執行後的時間
	LARGE_INTEGER liPerfNow = { 0 };
	QueryPerformanceCounter(&liPerfNow);
	// 計算出 Total 所需要的時間 (millisecond)
	long decodeDulation = (((liPerfNow.QuadPart - m_liPerfStart.QuadPart) * 1000) / m_liPerfFreq.QuadPart);
	// print 			
	printf("Total 執行時間 %d ms \n", decodeDulation);

	// 計算出 平行化區塊值行時間 (millisecond)
	long decodeDulation_NoFileIO = (((liPerfNow_NoFileIo.QuadPart - m_liPerfStart_NoFileIo.QuadPart) * 1000) / m_liPerfFreq.QuadPart);
	// print 			
	printf("平行化區塊執行時間 %d ms \n", decodeDulation_NoFileIO);

	return 0;
}
