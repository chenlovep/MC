#include <vtkVersion.h>
#include <vtkSmartPointer.h>
#include <vtkMarchingCubes.h>
#include <vtkVoxelModeller.h>
#include <vtkSphereSource.h>
#include <vtkImageData.h>
#include <vtkPNGReader.h>
#include <vtkSmoothPolyDataFilter.h>
#include <vtkActor.h>
#include <vtkProperty.h>
#include <vtkPolyDataMapper.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkStripper.h>
#include <vtkJPEGReader.h>
#include <vtkCamera.h>
#include <vtkWindowedSincPolyDataFilter.h>
#include <iostream>
#include "vtkMarchingCubesSpace.h"
#include "vtkAutoInit.h"
#include <time.h>
VTK_MODULE_INIT(vtkRenderingOpenGL);
VTK_MODULE_INIT(vtkInteractionStyle);
//vtkSmartPointer相当于一个智能指针
int main(int argc, char *argv[]) {
	clock_t start = clock();
	//设置一种格式
	//二直化图片
	vtkSmartPointer<vtkImageData> BinaryVolume = vtkSmartPointer<vtkImageData>::New();
	//设置等值面的值
	double isoValue = 250;

	//读取序列图片（二值化）
	vtkSmartPointer<vtkJPEGReader> ReaderBinary = vtkSmartPointer<vtkJPEGReader>::New();
	ReaderBinary->SetDataScalarTypeToUnsignedChar();
	ReaderBinary->SetFileDimensionality(2);
	ReaderBinary->SetFilePrefix("E:\\biye\\biye\\mimics\\picture_1\\picture_");
	ReaderBinary->SetFileNameSliceSpacing(1);
	ReaderBinary->SetFilePattern("%s%02d.jpg");
	ReaderBinary->SetDataExtent(0, 127, 0, 127, 1, 18);
	ReaderBinary->SetDataSpacing(1,1,1);//设置三维数据场中像素间距
	ReaderBinary->Update();

	BinaryVolume->DeepCopy(ReaderBinary->GetOutput());

	

	//调用系统立方体六面体
	vtkSmartPointer<vtkMarchingCubes> surface = vtkSmartPointer<vtkMarchingCubes>::New();
#if VTK_MAJOR_VERSION <= 5
	surface->SetInput(BinaryVolume);
#else 
	//属于管道连接
	surface->SetInputData(BinaryVolume); //0号端口作为二值化图像输入

#endif
	surface->ComputeNormalsOff();//off法向量默认为输入二值化图像的法向量计算
	surface->SetValue(0, isoValue); //设置等值面数据

	vtkSmartPointer<vtkStripper> stripper = vtkSmartPointer<vtkStripper>::New();
	stripper->SetInputConnection(surface->GetOutputPort()); //将生成的三角片连接成三角带
	stripper->Update();

	//vtkSmartPointer<vtkSmoothPolyDataFilter> smoothFilter = vtkSmartPointer<vtkSmoothPolyDataFilter>::New();
	//smoothFilter->SetInputConnection(stripper->GetOutputPort());
	//smoothFilter->SetNumberOfIterations(200);//
	//smoothFilter->Update();

	//vtkSmartPointer<vtkWindowedSincPolyDataFilter> wndSincsmoothFilter = vtkSmartPointer<vtkWindowedSincPolyDataFilter>::New();
	//wndSincsmoothFilter->SetInputConnection(stripper->GetOutputPort());
	//wndSincsmoothFilter->SetNumberOfIterations(3);
	//wndSincsmoothFilter->Update();

	vtkSmartPointer<vtkPolyDataMapper> mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
	mapper->SetInputConnection(stripper->GetOutputPort());
	//mapper->SetInputConnection(wndSincsmoothFilter->GetOutputPort());
	//mapper->SetInputConnection(smoothFilter->GetOutputPort());
	mapper->ScalarVisibilityOff();//

	vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
	actor->SetMapper(mapper);
	actor->GetProperty()->SetColor(1, 0, 0);
	actor->GetProperty()->SetDiffuseColor(1, 0, 0);//设置图像反光照射的颜色，反光颜色为setdiffusecolor+setcolor
												   //setDiffusecolor会覆盖之前setcolor的颜色，重叠部分进行混加
	actor->GetProperty()->SetSpecular(1);
	actor->GetProperty()->SetSpecularPower(10);


	vtkCamera *aCamera = vtkCamera::New();
	aCamera->SetViewUp(0, 0, -1);
	aCamera->SetPosition(0,1,0);
	aCamera->SetFocalPoint(0,0,0);



	vtkSmartPointer<vtkRenderer> renderer = vtkSmartPointer<vtkRenderer>::New();
	//renderer->SetActiveCamera(aCamera);
	renderer->ResetCamera();
	aCamera->Dolly(1.5);
	renderer->SetBackground(1,1,1);
	renderer->ResetCameraClippingRange();

	vtkSmartPointer<vtkRenderWindow> renderWindow = vtkSmartPointer<vtkRenderWindow>::New();
	renderWindow->AddRenderer(renderer);
	vtkSmartPointer<vtkRenderWindowInteractor> interactor = vtkSmartPointer<vtkRenderWindowInteractor>::New();
	interactor->SetRenderWindow(renderWindow);

	renderer->AddActor(actor);

	renderWindow->Render();
	clock_t end = clock();
	cout << "所用时间:" << (double)(end-start) / CLOCKS_PER_SEC << endl;
	interactor->Start();
	

	return 0;
}