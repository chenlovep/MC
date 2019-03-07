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
#include <iostream>
#include "vtkMarchingCubesSpace.h"
#include "vtkAutoInit.h"
#include <time.h>
VTK_MODULE_INIT(vtkRenderingOpenGL);
VTK_MODULE_INIT(vtkInteractionStyle);
//vtkSmartPointer�൱��һ������ָ��
int main(int argc, char *argv[]) {
	clock_t start = clock();
	//����һ�ָ�ʽ
	//��ֱ��ͼƬ
	vtkSmartPointer<vtkImageData> BinaryVolume = vtkSmartPointer<vtkImageData>::New();
	//���õ�ֵ���ֵ
	double isoValue = 250;

	//��ȡ����ͼƬ����ֵ����
	vtkSmartPointer<vtkJPEGReader> ReaderBinary = vtkSmartPointer<vtkJPEGReader>::New();
	ReaderBinary->SetDataScalarTypeToUnsignedChar();
	ReaderBinary->SetFileDimensionality(2);
	ReaderBinary->SetFilePrefix("E:\\biye\\biye\\mimics\\picture_1\\picture_");
	ReaderBinary->SetFileNameSliceSpacing(1);
	ReaderBinary->SetFilePattern("%s%02d.jpg");
	ReaderBinary->SetDataExtent(0, 127, 0, 127, 1, 18);
	ReaderBinary->SetDataSpacing(1, 1, 1);//������ά���ݳ������ؼ��
	ReaderBinary->Update();

	BinaryVolume->DeepCopy(ReaderBinary->GetOutput());

	

	//����ϵͳ������������
	vtkSmartPointer<vtkMarchingCubesSpace> surface = vtkSmartPointer<vtkMarchingCubesSpace>::New();
#if VTK_MAJOR_VERSION <= 5
	surface->SetInput(BinaryVolume);
#else 
	//���ڹܵ�����
	surface->SetInputData(BinaryVolume); //0�Ŷ˿���Ϊ��ֵ��ͼ������

#endif
	surface->ComputeNormalsOff();//off������Ĭ��Ϊ�����ֵ��ͼ��ķ���������
	surface->SetValue(0, isoValue); //���õ�ֵ������

	vtkSmartPointer<vtkStripper> stripper = vtkSmartPointer<vtkStripper>::New();
	stripper->SetInputConnection(surface->GetOutputPort()); //�����ɵ�����Ƭ���ӳ����Ǵ�
	stripper->Update();

	vtkSmartPointer<vtkSmoothPolyDataFilter> smoothFilter = vtkSmartPointer<vtkSmoothPolyDataFilter>::New();
	smoothFilter->SetInputConnection(stripper->GetOutputPort());
	smoothFilter->SetNumberOfIterations(5);//
	smoothFilter->Update();

	vtkSmartPointer<vtkPolyDataMapper> mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
	mapper->SetInputConnection(smoothFilter->GetOutputPort());
	mapper->ScalarVisibilityOff();//

	vtkSmartPointer<vtkActor> actor = vtkSmartPointer<vtkActor>::New();
	actor->SetMapper(mapper);
	actor->GetProperty()->SetColor(1, 0, 0);
	actor->GetProperty()->SetDiffuseColor(1, 0, 0);//����ͼ�񷴹��������ɫ��������ɫΪsetdiffusecolor+setcolor
												   //setDiffusecolor�Ḳ��֮ǰsetcolor����ɫ���ص����ֽ��л��
	actor->GetProperty()->SetSpecular(1);
	actor->GetProperty()->SetSpecularPower(10);


	vtkSmartPointer<vtkRenderer> renderer = vtkSmartPointer<vtkRenderer>::New();
	renderer->SetBackground(1, 1, 1);

	vtkSmartPointer<vtkRenderWindow> renderWindow = vtkSmartPointer<vtkRenderWindow>::New();
	renderWindow->AddRenderer(renderer);
	vtkSmartPointer<vtkRenderWindowInteractor> interactor = vtkSmartPointer<vtkRenderWindowInteractor>::New();
	interactor->SetRenderWindow(renderWindow);

	renderer->AddActor(actor);

	renderWindow->Render();
	clock_t end = clock();
	cout << "����ʱ��:" << (double)(end-start) / CLOCKS_PER_SEC << endl;
	interactor->Start();
	

	return 0;
}