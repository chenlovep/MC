#include "vtkMarchingCubesSpace.h"
#include "vtkCellArray.h"
#include "vtkCharArray.h"
#include "vtkDoubleArray.h"
#include "vtkFloatArray.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkIntArray.h"
#include "vtkLongArray.h"
#include "vtkMarchingCubesSpaceCases.h"
#include "vtkMath.h"
#include "vtkMergePoints.h"
#include "vtkObjectFactory.h"
#include "vtkPointData.h"
#include "vtkPolyData.h"
#include "vtkShortArray.h"
#include "vtkStructuredPoints.h"
#include "vtkUnsignedCharArray.h"
#include "vtkUnsignedIntArray.h"
#include "vtkUnsignedLongArray.h"
#include "vtkUnsignedShortArray.h"
#include "vtkStreamingDemandDrivenPipeline.h"
#include "vtkIncrementalPointLocator.h"
#include "math.h"
#include <cmath>
#include <stdlib.h>
#include<queue>
#include<vector>

using namespace std;
vtkStandardNewMacro(vtkMarchingCubesSpace);

// Description:
// Construct object with initial range (0,1) and single contour value
// of 0.0. ComputeNormal is on, ComputeGradients is off and ComputeScalars is on.
// �����ʼ��Χ(0,1)��������ֵΪ0.0�Ķ���
// ��computennormal
// �ر�ComputeGradients
// ��ComputeScalars��
vtkMarchingCubesSpace::vtkMarchingCubesSpace()
{
  this->ContourValues = vtkContourValues::New();
  this->ComputeNormals = 1;
  this->ComputeGradients = 0;
  this->ComputeScalars = 1;
  this->Locator = NULL;
  this->SetNumberOfInputPorts(2);
}

vtkMarchingCubesSpace::~vtkMarchingCubesSpace()
{
  this->ContourValues->Delete();
  if ( this->Locator )
    {
    this->Locator->UnRegister(this);
    this->Locator = NULL;
    }
}

// Description:
// Overload standard modified time function. If contour values are modified,
// then this object is modified as well.
unsigned long vtkMarchingCubesSpace::GetMTime()
{
  unsigned long mTime=this->Superclass::GetMTime();
  unsigned long mTime2=this->ContourValues->GetMTime();

  mTime = ( mTime2 > mTime ? mTime2 : mTime );
  if (this->Locator)
    {
    mTime2=this->Locator->GetMTime();
    mTime = ( mTime2 > mTime ? mTime2 : mTime );
    }

  return mTime;
}

// Calculate the gradient using central difference.
// �����Ĳ�ּ����ݶ�
// NOTE: We calculate the negative of the gradient for efficiency
template <class T> 
//i,j,k�������꣬s�Ҷ�ֵ
//dims�� slicesize ��spacing��n�ݶ�
void vtkMarchingCubesSpaceComputePointGradient(int i, int j, int k, T *s, int dims[3],
                                          vtkIdType sliceSize, double spacing[3], double n[3])
{
  double sp, sm;
  //n[0],n[1],n[2]����ÿ��������x,y,z����������ݶ�ֵ
  // x-direction
  if ( i == 0 )
    {
    sp = s[i+1 + j*dims[0] + k*sliceSize];    // dims[0] stores xmax,dims[1] stores ymax,dims[2] stores zmax
    sm = s[i + j*dims[0] + k*sliceSize];
    n[0] = (sm - sp) / spacing[0];  //spacing[0] stores delta x;spacing[2] stores delta y,spacing[3] stores delta z
    }
  else if ( i == (dims[0]-1) )
    {
    sp = s[i + j*dims[0] + k*sliceSize];
    sm = s[i-1 + j*dims[0] + k*sliceSize];
    n[0] = (sm - sp) / spacing[0];
    }
  else
    {
    sp = s[i+1 + j*dims[0] + k*sliceSize];
    sm = s[i-1 + j*dims[0] + k*sliceSize];
    n[0] = 0.5 * (sm - sp) / spacing[0];
    }

  // y-direction
  if ( j == 0 )
    {
    sp = s[i + (j+1)*dims[0] + k*sliceSize];
    sm = s[i + j*dims[0] + k*sliceSize];
    n[1] = (sm - sp) / spacing[1];
    }
  else if ( j == (dims[1]-1) )
    {
    sp = s[i + j*dims[0] + k*sliceSize];
    sm = s[i + (j-1)*dims[0] + k*sliceSize];
    n[1] = (sm - sp) / spacing[1];
    }
  else
    {
    sp = s[i + (j+1)*dims[0] + k*sliceSize];
    sm = s[i + (j-1)*dims[0] + k*sliceSize];
    n[1] = 0.5 * (sm - sp) / spacing[1];
    }

  // z-direction
  if ( k == 0 )
    {
    sp = s[i + j*dims[0] + (k+1)*sliceSize];
    sm = s[i + j*dims[0] + k*sliceSize];
    n[2] = (sm - sp) / spacing[2];
    }
  else if ( k == (dims[2]-1) )
    {
    sp = s[i + j*dims[0] + k*sliceSize];
    sm = s[i + j*dims[0] + (k-1)*sliceSize];
    n[2] = (sm - sp) / spacing[2];
    }
  else
    {
    sp = s[i + j*dims[0] + (k+1)*sliceSize];
    sm = s[i + j*dims[0] + (k-1)*sliceSize];
    n[2] = 0.5 * (sm - sp) / spacing[2];
    }
}







// Contouring filter specialized for volumes and "short int" data values.
//
template <class T>
void vtkMarchingCubesSpaceComputeGradient(vtkMarchingCubesSpace *self,T *scalars, int dims[3],
                                     double origin[3], double spacing[3],
                                     vtkIncrementalPointLocator *locator,
                                     vtkDataArray *newScalars,
                                     vtkDataArray *newGradients,
                                     vtkDataArray *newNormals,
                                     vtkCellArray *newPolys, double *values,
                                     int numValues)
{
  double s[8], value;
  int i, j, k;
  vtkIdType sliceSize;
  static int CASE_MASK[8] = {1,2,4,8,16,32,64,128};  //��0000 0001,000 0010�ȵȣ���index�����㣬index��cube�˸�����״̬
 vtkMarchingCubesSpaceTriangleCases *triCase, *triCases;  //�ṹ��vtkMarchingCubesSpaceTriangleCases����edges[16](cube������Ӧ��triangle)��ָ��ṹ��ָ�뺯��
  EDGE_LIST  *edge;
  int contNum, jOffset, ii, index, *vert;
  vtkIdType kOffset, idx;
  vtkIdType ptIds[3];
  int ComputeNormals = newNormals != NULL;
  int ComputeGradients = newGradients != NULL;
  int ComputeScalars = newScalars != NULL;
  int NeedGradients;
  int extent[6];
  int count = 1;
  double t, *x1, *x2, x[3], *n1, *n2, n[3], min, max;
  double pts[8][3], gradients[8][3], xp, yp, zp;      //pts[8][3]�У�8��cube�˸����㣬3�Ƕ�Ӧx,y,z����
  static int edges[12][2] = { {0,1}, {1,2}, {3,2}, {0,3},
                              {4,5}, {5,6}, {7,6}, {4,7},
                              {0,4}, {1,5}, {3,7}, {2,6}};  // 0-7��cube�����ţ�{0,1}��ʾ��0�Ŷ����1�Ŷ���֮�����

  vtkInformation *inInfo = self->GetExecutive()->GetInputInformation(0, 0); //Get this algorithm's executive,Get the pipeline information for the given input connection.
  inInfo->Get(vtkStreamingDemandDrivenPipeline::WHOLE_EXTENT(),extent);

  triCases = vtkMarchingCubesSpaceTriangleCases::GetCases();  //�õ�vtkMarchingCubesSpaceTriangleCases.cxx�о�̬��Ա�����͡������ʷ֡����ұ��ͷָ��

//
// Get min/max contour values
//
  if ( numValues < 1 )
    {
    return;
    }
  for ( min=max=values[0], i=1; i < numValues; i++)
    {
    if ( values[i] < min )
      {
      min = values[i];
      }
    if ( values[i] > max )
      {
      max = values[i];
      }
    }
//
// Traverse all voxel cells, generating triangles and point gradients
// using marching cubes algorithm.
//
  sliceSize = dims[0] * dims[1];
  for ( k=0; k < (dims[2]-1); k++)
    {
    self->UpdateProgress (k / static_cast<double>(dims[2] - 1));
    if (self->GetAbortExecute())
      {
      break;
      }
    kOffset = k*sliceSize;
    pts[0][2] = origin[2] + (k+extent[4]) * spacing[2];
    zp = pts[0][2] + spacing[2];
    for ( j=0; j < (dims[1]-1); j++)
      {
      jOffset = j*dims[0];
      pts[0][1] = origin[1] + (j+extent[2]) * spacing[1];
      yp = pts[0][1] + spacing[1];
      for ( i=0; i < (dims[0]-1); i++)
        {
        //get scalar values
        idx = i + jOffset + kOffset;
        s[0] = scalars[idx];                          //point(i,j,k)��Ӧ����
        s[1] = scalars[idx+1];                       //point(i+1,j,k����Ӧ����
        s[2] = scalars[idx+1 + dims[0]];             //point(i+1,j+1,k)��Ӧ����
        s[3] = scalars[idx + dims[0]];               //point(i,j+1,k)��Ӧ����
        s[4] = scalars[idx + sliceSize];             //point(i,j,k+1)��Ӧ����
        s[5] = scalars[idx+1 + sliceSize];           //point(i+1,j,k+1)��Ӧ����
        s[6] = scalars[idx+1 + dims[0] + sliceSize]; //point(i+1,j+1,k+1)��Ӧ����
        s[7] = scalars[idx + dims[0] + sliceSize];   //point(i,j+1,k+1)��Ӧ����

        if ( (s[0] < min && s[1] < min && s[2] < min && s[3] < min &&
        s[4] < min && s[5] < min && s[6] < min && s[7] < min) ||
        (s[0] > max && s[1] > max && s[2] > max && s[3] > max &&
        s[4] > max && s[5] > max && s[6] > max && s[7] > max) )
          {
          continue; // no contours possible
          }

        //create voxel points
        pts[0][0] = origin[0] + (i+extent[0]) * spacing[0];
        xp = pts[0][0] + spacing[0];

        pts[1][0] = xp;
        pts[1][1] = pts[0][1];
        pts[1][2] = pts[0][2];

        pts[2][0] = xp;
        pts[2][1] = yp;
        pts[2][2] = pts[0][2];

        pts[3][0] = pts[0][0];
        pts[3][1] = yp;
        pts[3][2] = pts[0][2];

        pts[4][0] = pts[0][0];
        pts[4][1] = pts[0][1];
        pts[4][2] = zp;

        pts[5][0] = xp;
        pts[5][1] = pts[0][1];
        pts[5][2] = zp;

        pts[6][0] = xp;
        pts[6][1] = yp;
        pts[6][2] = zp;

        pts[7][0] = pts[0][0];
        pts[7][1] = yp;
        pts[7][2] = zp;           //cube�˸���������

        NeedGradients = ComputeGradients || ComputeNormals;

        //create gradients if needed
        if (NeedGradients)
          {
         vtkMarchingCubesSpaceComputePointGradient(i,j,k, scalars, dims, sliceSize, spacing, gradients[0]);  //ǰ�涨����double gradients[8][3]
         vtkMarchingCubesSpaceComputePointGradient(i+1,j,k, scalars, dims, sliceSize, spacing, gradients[1]);
         vtkMarchingCubesSpaceComputePointGradient(i+1,j+1,k, scalars, dims, sliceSize, spacing, gradients[2]);
         vtkMarchingCubesSpaceComputePointGradient(i,j+1,k, scalars, dims, sliceSize, spacing, gradients[3]);
         vtkMarchingCubesSpaceComputePointGradient(i,j,k+1, scalars, dims, sliceSize, spacing, gradients[4]);
         vtkMarchingCubesSpaceComputePointGradient(i+1,j,k+1, scalars, dims, sliceSize, spacing, gradients[5]);
         vtkMarchingCubesSpaceComputePointGradient(i+1,j+1,k+1, scalars, dims, sliceSize, spacing, gradients[6]);
         vtkMarchingCubesSpaceComputePointGradient(i,j+1,k+1, scalars, dims, sliceSize, spacing, gradients[7]);
          }
        for (contNum=0; contNum < numValues; contNum++)
          {
          value = values[contNum];
          // Build the case table
          for ( ii=0, index = 0; ii < 8; ii++)
            {
            if ( s[ii] >= value )
              {
              index |= CASE_MASK[ii];  //index��cube�������ţ�0-255����ʾvertex�˸�״̬��
              }
            }
          if ( index == 0 || index == 255 ) //no surface
            {
            continue;
            }

          triCase = triCases+ index; //triCases��vtkMarchingCubesSpaceTriangleCases.cxx�С����͡������ʷ֡����ұ��ͷָ��,
          edge = triCase->edges; //edges��vtkMarchingCubesSpaceTriangleCases�ṹ���ԱEDGE_LIST edges[16];edges[16]ÿ��Ԫ�ض�Ӧcube�ߵ����;edge��edges[16]��ͷָ��

          for ( ; edge[0] > -1; edge += 3 )  //ÿ�����߾���һ��triangle������edge+=3
            {
			  count = count + 1;
            for (ii=0; ii<3; ii++) //insert triangle
			 {
              vert = edges[edge[ii]];   //������ʼ����edges[12][2] = {{0,1},{1,2},{3,2},{0,3},{4,5},{5,6},{7,6},{4,7},{0,4},{1,5},{3,7},{2,6}};vert��ÿ�����Ӧ��ͷָ��
              //t = (value - s[vert[0]]) / (s[vert[1]] - s[vert[0]]);
              x1 = pts[vert[0]];   //������ʼ����pts[8][3]
              x2 = pts[vert[1]];
			  x[0] =0.5*(x1[0] + x2[0]);
              x[1] =0.5*(x1[1] + x2[1]);
              x[2] =0.5*(x1[2] + x2[2]);       //���Բ�ֵ��Ϊȡ�е�

              // check for a new point
              if ( locator->InsertUniquePoint(x, ptIds[ii]) ) //Determine whether point given by x[3] has been inserted into points list
                  {                                           //Return 0 if point was already in the list, otherwise return 1.-- 
                  if (NeedGradients)                          //---If the point was not in the list, it will be ADDED
                    {
                    n1 = gradients[vert[0]];
                    n2 = gradients[vert[1]];
                    n[0] = 0.5*(n1[0] + n2[0]);
                    n[1] = 0.5*(n1[1] + n2[1]);
                    n[2] = 0.5*(n1[2] + n2[2]);    //�ݶȼ����Ϊȡƽ��
                    }
                  if (ComputeScalars)
                    {
                    newScalars->InsertTuple(ptIds[ii],&value);
                    }
                  if (ComputeGradients)
                    {
                    newGradients->InsertTuple(ptIds[ii],n);
                    }
                  if (ComputeNormals)
                    {
                    vtkMath::Normalize(n);
                    newNormals->InsertTuple(ptIds[ii],n);
                    }
                  }
			}
            // check for degenerate triangle
            if ( ptIds[0] != ptIds[1] &&
                 ptIds[0] != ptIds[2] &&
                 ptIds[1] != ptIds[2] )
                {
                newPolys->InsertNextCell(3,ptIds);
                }
			
            }//for each triangle

          }//for all contours
        }//for i
      }//for j
    }//for k
	cout << "������Ƭ����:" << count << endl;
}


/*
//
// MyvtkMarchingCubesSpaceComputeGradient
//

template <class T>
void vtkMarchingCubesSpaceComputeGradient(vtkMarchingCubesSpace *self,T *scalars,int dims[3],   //scalars1��Ϊԭʼͼ��ı�������
										  double origin[3], double spacing[3],
										  vtkIncrementalPointLocator *locator,
										  vtkDataArray *newScalars,
										  vtkDataArray *newGradients,
										  vtkDataArray *newNormals,
										  vtkCellArray *newPolys, double *values,
										  int numValues)
{
	// GetDimensions(dims);
	// dims����x,y,z���������������������
	// ͼ��ԭ��origin
	double s[8], value;
	int i, j, k;
	vtkIdType sliceSize;
	static int CASE_MASK[8] = {1,2,4,8,16,32,64,128};  //��0000 0001,000 0010�ȵȣ���index�����㣬index��cube�˸�����״̬
	vtkMarchingCubesSpaceTriangleCases *triCase, *triCases;  //�ṹ��vtkMarchingCubesSpaceTriangleCases����edges[16](cube������Ӧ��triangle)��ָ��ṹ��ָ�뺯��
	                                                         //MC�㷨�еġ������ʷ֡����ұ�triCases
	EDGE_LIST  *edge;
	int contNum, jOffset, ii, index, *vert;
	vtkIdType kOffset, idx;
	vtkIdType ptIds[3];
	int ComputeNormals = newNormals != NULL;
	int ComputeGradients = newGradients != NULL;
	int ComputeScalars = newScalars != NULL;
	int NeedGradients;
	int extent[6];
	int M, N = 4, f_min = -10000;
	int a,b,X,Y,Z;
	int count=1;
	double t, *x1, *x2, x[3], *n1, *n2, n[3], min, max;
	double pts[8][3], gradients[8][3], xp, yp, zp;      //pts[8][3]�У�8��cube�˸����㣬3�Ƕ�Ӧx,y,z����, gradients
	static int edges[12][2] = { {0,1}, {1,2}, {3,2}, {0,3},
	{4,5}, {5,6}, {7,6}, {4,7},
	{0,4}, {1,5}, {3,7}, {2,6}};  // 0-7��cube�����ţ�{0,1}��ʾ��0�Ŷ����1�Ŷ���֮�����
	
	vtkMarchingCubesSpaceNeighbourCases *neiborCase, *neiborCases;  //�ṹ��vtkMarchingCubesSpaceNeighbourCases����neighbours[6](cube���ھ�)��ָ��ṹ��ָ�뺯��
																	//�������ڽӱ�neiborCases
	bool  *neighbour;
	std::queue<Cube> CubeQueue;  //����
	std::vector<Cube> List(dims[0]*dims[1]*dims[2]);//�����ɸ�Cube�ṹ����ɵ�����List	


	vtkInformation *inInfo = self->GetExecutive()->GetInputInformation(0, 0); //Get this algorithm's executive,Get the pipeline information for the given input connection.
	//extent��������ͼƬ����(0,image_weight, image_height, 0, image_num)
	inInfo->Get(vtkStreamingDemandDrivenPipeline::WHOLE_EXTENT(),extent);

	triCases =  vtkMarchingCubesSpaceTriangleCases::GetCases();  //�õ�vtkMarchingCubesSpaceCases.cxx�о�̬��Ա�����͡������ʷ֡����ұ��ͷָ��
	neiborCases = vtkMarchingCubesSpaceNeighbourCases::GetCases();  //�õ�vtkMarchingCubesSpaceCases.cxx�о�̬��Ա�ڽӲ��ұ��ͷָ��

	//
	// Get min/max contour values
	//
	if ( numValues < 1 )
	{
		return;
	}
	for ( min=max=values[0], i=1; i < numValues; i++)
	{
		if ( values[i] < min )
		{
			min = values[i];
		}
		if ( values[i] > max )
		{
			max = values[i];
		}
   	}


	//
	// ����index��Ϊ0��255��������
	// dims[0]->��Ƭ��x�����Ϻ��е���������;dims[1]->��Ƭ��y�����Ϻ��е���������
	sliceSize = dims[0] * dims[1];//ͼ���������������
	//fprintf("sliceSize:%d", sliceSize);
	//extent(0,row, 0,col,0,num)
	for ( k=0; k < (dims[2]-1); k++)//��ͼ�������������д���
	{
		self->UpdateProgress (k / static_cast<double>(dims[2] - 1));
		if (self->GetAbortExecute())
		{
			break;
		}
		//sliceSizeȷ��ͼ��ĺ������С
		kOffset = k*sliceSize;
		//ptsΪpts[8][3]ȷ��ÿ������Ķ˵�����
		//originԭʼͼ
		//spacingΪÿ������ϵ�Ĳ�������
		pts[0][2] = origin[2] + (k*spacing[2]);//+extent[4]) * spacing[2];
		zp = pts[0][2] + spacing[2];
		for ( j=0; j < (dims[1]-1); j++)
		{
			jOffset = j*dims[0];
			pts[0][1] = origin[1] + (j*spacing[1]);//+extent[2]) * spacing[1];
			yp = pts[0][1] + spacing[1];
			for ( i=0; i < (dims[0]-1); i++)
			{
				//get scalar values
				idx = i + jOffset + kOffset;
				s[0] = scalars[idx];                          //point(i,j,k)��Ӧ����
				s[1] = scalars[idx+1];                       //point(i+1,j,k����Ӧ����
				s[2] = scalars[idx+1+dims[0]];             //point(i+1,j+1,k)��Ӧ����
				s[3] = scalars[idx+dims[0]];               //point(i,j+1,k)��Ӧ����
				s[4] = scalars[idx+sliceSize];             //point(i,j,k+1)��Ӧ����
				s[5] = scalars[idx+1+sliceSize];           //point(i+1,j,k+1)��Ӧ����
				s[6] = scalars[idx+1+dims[0]+sliceSize]; //point(i+1,j+1,k+1)��Ӧ����
				s[7] = scalars[idx+dims[0]+sliceSize];   //point(i,j+1,k+1)��Ӧ����

				if ((s[0] < min && s[1] < min && s[2] < min && s[3] < min &&
					s[4] < min && s[5] < min && s[6] < min && s[7] < min) ||
					(s[0] > max && s[1] > max && s[2] > max && s[3] > max &&
					s[4] > max && s[5] > max && s[6] > max && s[7] > max))
				{
					continue; //no contours possible
				} 
				for (contNum=0; contNum < numValues; contNum++)
				{
					//��ֵ��ֵvalue
					//s[8]
					value = values[contNum];
					// Build the case table
					for ( ii=0, index = 0; ii < 8; ii++)
					{
						if ( s[ii] >= value )
						{
							//CASE_MASK[1,2,4,8,16,32,64,128]������ģ���㣬�õ�0-255����ͬ��������index
							//CASE_MASK[00000001, 00000010, 00000100,00001000, 00010000, 00010000, 00100000, 01000000, 10000000]
							index |= CASE_MASK[ii];  //index��cube�������ţ�0-255����ʾvertex�˸�״̬��
						}
					}
					//index = 0 or index = 255��ʾ���������ֵ���޽���
					if ( index == 0 || index == 255 ) //no surface
					{
						continue;
					}
					goto FindSeedPoint;
					
				}//for all contours
			}//for i
		}//for j
	}//for k



FindSeedPoint:
	//Cub�ṹ��
	//flag��������Ϊ0(�ж��Ƿ���1����0δ����)
	bool flag=0;Cube CubeTemp;
	CubeQueue.push(Cube(i,j,k,index,flag));
	//CubeQueue
	while(!CubeQueue.empty()){
		//�Ӷ�����ȡ������Ԫ��
		CubeTemp=CubeQueue.front();    
		//CubeTemp.oi, CubeTemp.oj, CubeTemp.ok�������������0�����ص�������Ϣ
		//CubeTemp.index����ǰ�����������index
		i=CubeTemp.oi;j=CubeTemp.oj;k=CubeTemp.ok;index=CubeTemp.index;
		self->UpdateProgress (k / static_cast<double>(dims[2] - 1));
		if (self->GetAbortExecute())
		{
			break;
		}
		
		kOffset = k*sliceSize;
		pts[0][2] = origin[2] + (k*spacing[2]);//+extent[4]) * spacing[2];
		zp = pts[0][2] + spacing[2];
		jOffset = j*dims[0];
		pts[0][1] = origin[1] + (j*spacing[1]);// +extent[2]) * spacing[1];
		yp = pts[0][1] + spacing[1];

		//������ı��idx
		idx = i + jOffset + kOffset;
		CubeQueue.pop();
		if (List[idx].Flag == 1)
			continue;
		//�������������óɴ����
		List[idx].Flag=1;


		s[0] = scalars[idx];                          //point(i,j,k)��Ӧ����
		s[1] = scalars[idx+1];                       //point(i+1,j,k����Ӧ����
		s[2] = scalars[idx+1 + dims[0]];             //point(i+1,j+1,k)��Ӧ����
		s[3] = scalars[idx + dims[0]];               //point(i,j+1,k)��Ӧ����
		s[4] = scalars[idx + sliceSize];             //point(i,j,k+1)��Ӧ����
		s[5] = scalars[idx+1 + sliceSize];           //point(i+1,j,k+1)��Ӧ����
		s[6] = scalars[idx+1 + dims[0] + sliceSize]; //point(i+1,j+1,k+1)��Ӧ����
		s[7] = scalars[idx + dims[0] + sliceSize];   //point(i,j+1,k+1)��Ӧ����

		if ( (s[0] < min && s[1] < min && s[2] < min && s[3] < min &&
			s[4] < min && s[5] < min && s[6] < min && s[7] < min) ||
			(s[0] > max && s[1] > max && s[2] > max && s[3] > max &&
			s[4] > max && s[5] > max && s[6] > max && s[7] > max) )
		{
			continue; // no contours possible
		}


		//create voxel points
		//�����µ�������������Ϣpts[8][3]
		pts[0][0] = origin[0] + (i + spacing[0]);// extent[0]) * spacing[0];
		xp = pts[0][0] + spacing[0];

		pts[1][0] = xp;
		pts[1][1] = pts[0][1];
		pts[1][2] = pts[0][2];

		pts[2][0] = xp;
		pts[2][1] = yp;
		pts[2][2] = pts[0][2];

		pts[3][0] = pts[0][0];
		pts[3][1] = yp;
		pts[3][2] = pts[0][2];

		pts[4][0] = pts[0][0];
		pts[4][1] = pts[0][1];
		pts[4][2] = zp;

		pts[5][0] = xp;
		pts[5][1] = pts[0][1];
		pts[5][2] = zp;

		pts[6][0] = xp;
		pts[6][1] = yp;
		pts[6][2] = zp;

		pts[7][0] = pts[0][0];
		pts[7][1] = yp;
		pts[7][2] = zp;

		//���ݵ�ǰindex��Ӧ�������ʷ֡���ֵ��ȷ����ǰ�����ʷ���ʽ
		NeedGradients = ComputeGradients || ComputeNormals;

		//create gradients if needed
		if (NeedGradients)
		{
			//vtkMarchingCubesSpaceComputePointGradient()
			//ȷ��ÿ����������ÿ��������ݶ�ֵ
			//gradients[8][3]
			vtkMarchingCubesSpaceComputePointGradient(i,j,k, scalars, dims, sliceSize, spacing, gradients[0]);   //ǰ�涨����double gradients[8][3]
			vtkMarchingCubesSpaceComputePointGradient(i+1,j,k, scalars, dims, sliceSize, spacing, gradients[1]);
			vtkMarchingCubesSpaceComputePointGradient(i+1,j+1,k, scalars, dims, sliceSize, spacing, gradients[2]);
			vtkMarchingCubesSpaceComputePointGradient(i,j+1,k, scalars, dims, sliceSize, spacing, gradients[3]);
			vtkMarchingCubesSpaceComputePointGradient(i,j,k+1, scalars, dims, sliceSize, spacing, gradients[4]);
			vtkMarchingCubesSpaceComputePointGradient(i+1,j,k+1, scalars, dims, sliceSize, spacing, gradients[5]);
			vtkMarchingCubesSpaceComputePointGradient(i+1,j+1,k+1, scalars, dims, sliceSize, spacing, gradients[6]);
			vtkMarchingCubesSpaceComputePointGradient(i,j+1,k+1, scalars, dims, sliceSize, spacing, gradients[7]);
		}

		for (contNum=0; contNum < numValues; contNum++)
		{
			value = values[contNum];
			// Build the case table
			for ( ii=0, index = 0; ii < 8; ii++)
			{
				if ( s[ii] >= value )
				{
					index |= CASE_MASK[ii];  //index��cube�������ţ�0-255����ʾvertex�˸�״̬��
				}
			}
			if ( index == 0 || index == 255 ) //no surface
			{
				continue;
			}
			//vtkMarchingCubesSpaceTriangleCases *triCase, *triCases;  //�ṹ��vtkMarchingCubesSpaceTriangleCases����edges[16](cube������Ӧ��triangle)��ָ��ṹ��ָ�뺯��
	                                                                   //MC�㷨�еġ������ʷ֡����ұ�triCases
																	   //vtkMarchingCubesSpaceTriangleCases����edges[16]�Ľṹ������
			//edges[12][2]
			triCase = triCases+ index; //triCases��vtkMarchingCubesSpaceCases.cxx�С����͡������ʷ֡����ұ��ͷָ��,
			edge = triCase->edges;    //edges��vtkMarchingCubesSpaceCases�ṹ���ԱEDGE_LIST edges[16];edges[16]ÿ��Ԫ�ض�Ӧcube�ߵ����;edge��edges[16]��ͷָ��
			neiborCase = neiborCases+ index; //neiborCases��vtkMarchingCubesSpaceCases.cxx���ڽӲ��ұ��ͷָ��,
			neighbour = neiborCase->neighbours;    //neighbours��vtkMarchingCubesSpaceNeighbourCases�ṹ���Աbool neighbours[6];neighbour��neighbours[6]��ͷָ��

			//cout<<index<<endl;
			//edge->triCase��150
			for ( ; edge[0] > -1; edge += 3 )  //ÿ�����߾���һ��triangle������edge+=3
			{
				for (ii=0; ii<3; ii++) //insert triangle
				{
					//vertΪ������Ƭ����������ཻ�ıߵ���β����������Ӧ���Ժ�
					vert = edges[edge[ii]];   //������ʼ����edges[12][2] = {{0,1},{1,2},{3,2},{0,3},{4,5},{5,6},{7,6},{4,7},{0,4},{1,5},{3,7},{2,6}};vert��ÿ�����Ӧ��ͷָ��
					//t = (value - s[vert[0]]) / (s[vert[1]] - s[vert[0]]);
					//x1,x2ȷ���ཻ�ߵ�����
					x1 = pts[vert[0]];   //������ʼ����pts[8][3]
					x2 = pts[vert[1]];
					//���Բ�ֵ��
					//x[0] = x1[0] + (x1[0] + ((value)-s[vert[0]])*(x2[0] - x1[0])) / (s[vert[1]] - s[vert[0]]);
					//x[1] = x1[1] + (x1[1] + ((value)-s[vert[0]])*(x2[1] - x1[1])) / (s[vert[1]] - s[vert[0]]);
					//x[2] = x1[2] + (x1[2] + ((value)-s[vert[0]])*(x2[2] - x1[2])) / (s[vert[1]] - s[vert[0]]);
					//�зַ��������Բ�ֵ
					x[0] =0.5*(x1[0] + x2[0]);
					x[1] =0.5*(x1[1] + x2[1]);
					x[2] =0.5*(x1[2] + x2[2]);//���Բ�ֵ��Ϊȡ�е�(ȡÿ���ߵ��е�������Բ�ֵ)

					
					//N���ٽ�������Բ�ֵ
					for (int m = 0; m < pow(2,N)-1; m++) {
						a = int((pow(2, N) - (m + 1))*s[vert[0]] / pow(2, N) + (m + 1)*s[vert[1]] / pow(2, N));
					
						if (abs(a-value) < f_min)
							M = m;
							f_min = abs(a-value);
					}
					int X;
					if (x1[0] <= x2[0])
						X = x1[0];
					else
						X = x2[0];
					if (x1[1] <= x2[1])
						Y = x1[1];
					else
						Y = x2[1];
					if (x1[2] <= x2[2])
						Z = x1[2];
					else
						Z = x2[2];
					
					
					x[0] = (M + 1) / pow(2, N)*abs(x1[0] - x2[0]) + X;
					x[1] = (M + 1) / pow(2, N)*abs(x1[1] - x2[1]) + Y;
					x[2] = (M + 1) / pow(2, N)*abs(x1[2] - x2[2]) + Z;
					
					// check for a new point
					if ( locator->InsertUniquePoint(x, ptIds[ii]) )
					{
						if (NeedGradients)
						{
							//ȷ���µĵ���ݶ�
							n1 = gradients[vert[0]];
							n2 = gradients[vert[1]];

							n[0] = n1[0] + (n1[0] + ((value)-s[vert[0]])*(n2[0] - n1[0])) / (s[vert[1]] - s[vert[0]]);
							n[1] = n1[1] + (n1[1] + ((value)-s[vert[0]])*(n2[1] - n1[1])) / (s[vert[1]] - s[vert[0]]);
							n[2] = n1[2] + (n1[2] + ((value)-s[vert[0]])*(n2[2] - n1[2])) / (s[vert[1]] - s[vert[0]]);
							//n[0] = 0.5*(n1[0] + n2[0]);
							//n[1] = 0.5*(n1[1] + n2[1]);
							//n[2] = 0.5*(n1[2] + n2[2]);    //�ݶȼ����Ϊȥƽ��
							
							//n�ַ�ȷ���ݶ�
							
							if (n1[0] <= n2[0])
								X = n1[0];
							else
								X = n2[0];
							if (n1[1] <= n2[1])
								Y = n1[1];
							else
								Y = n2[1];
							if (n1[2] <= n2[2])
								Z = n1[2];
							else
								Z = n2[2];
							n[0] = (M + 1) / pow(2, N)*abs(n1[0] - n2[1]) + X;
							n[1] = (M + 1) / pow(2, N)*abs(n1[0] - n2[1]) + Y;
							n[2] = (M + 1) / pow(2, N)*abs(n1[0] - n2[1]) + Z;
							
						}
						if (ComputeScalars)
						{
							newScalars->InsertTuple(ptIds[ii],&value);
						}
						if (ComputeGradients)
						{
							newGradients->InsertTuple(ptIds[ii],n);
						}
						if (ComputeNormals)
						{
							vtkMath::Normalize(n);
							newNormals->InsertTuple(ptIds[ii],n);
						}
					}
				}
				// check for degenerate triangle
				if ( ptIds[0] != ptIds[1] && ptIds[0] != ptIds[2] && ptIds[1] != ptIds[2] )
				{
					newPolys->InsertNextCell(3,ptIds);
				}
			}//for each triangle
		}//for all contours

		//ȷ��������������ڵ�6���������Ƿ��нӴ����㣬��������������ӵ�������
		if(neighbour[0]){     //front direction
			if(i<(dims[0]-1)){        
				//i+1,j,k
				List[idx+1].oi=i+1;List[idx+1].oj=j;List[idx+1].ok=k;List[idx+1].index=index;
				count = count + 1;
				CubeQueue.push(List[idx+1]);
			}
		}
		if(neighbour[1]){   //back direction
			if(i>0){     
				//i-1,j,k
				List[idx-1].oi=i-1;List[idx-1].oj=j;List[idx-1].ok=k;List[idx-1].index=index;
				count = count + 1;
				CubeQueue.push(List[idx-1]);
			}
		}
		if(neighbour[2]){   //right direction
			if(j<(dims[1]-1)){ 
				//i,j+1,k
				List[idx + dims[0]].oi=i;List[idx + dims[0]].oj=j+1;List[idx + dims[0]].ok=k;List[idx + dims[0]].index=index;
				count = count + 1;
				CubeQueue.push(List[idx + dims[0]]);
			}
		}
		if(neighbour[3]){   //left direction
			if(j>0){     
				//i,j-1,k
				List[idx - dims[0]].oi=i;List[idx - dims[0]].oj=j-1;List[idx - dims[0]].ok=k;List[idx - dims[0]].index=index;
				count = count + 1;
				CubeQueue.push(List[idx - dims[0]]);
			}
		}
		if(neighbour[4]){       //up direction 
			if(k<(dims[2]-2)){        // ע��k<dims[2]-1�Ļ������������ԭ�㵽����ᳬ��
				//i,j,k+1
				List[idx + sliceSize].oi=i;List[idx + sliceSize].oj=j;List[idx + sliceSize].ok=k+1;List[idx + sliceSize].index=index;		
				count = count + 1;
				CubeQueue.push(List[idx + sliceSize]);
			}
		}
		if(neighbour[5]){  //left direction
			if(k>0){     
				//i,j,k-1
				List[idx - sliceSize].oi=i;List[idx - sliceSize].oj=j;List[idx - sliceSize].ok=k-1;List[idx - sliceSize].index=index;
				count = count + 1;
				CubeQueue.push(List[idx - sliceSize]);
			}	 
		}
	}//exit the queue
	cout << "������Ƭ������:" << count << endl;
}

*/




//
// Contouring filter specialized for volumes and "short int" data values.
//
int vtkMarchingCubesSpace::RequestData(vtkInformation *vtkNotUsed(request), vtkInformationVector **inputVector, vtkInformationVector *outputVector)
{
  // get the info objects
  vtkInformation *inInfo = inputVector[0]->GetInformationObject(0);

  vtkInformation *outInfo = outputVector->GetInformationObject(0);

  // get the input and output
  vtkImageData *input = vtkImageData::SafeDownCast(inInfo->Get(vtkDataObject::DATA_OBJECT()));

  vtkPolyData *output = vtkPolyData::SafeDownCast(outInfo->Get(vtkDataObject::DATA_OBJECT()));

  vtkPoints *newPts;
  vtkCellArray *newPolys;
  vtkFloatArray *newScalars;
  vtkFloatArray *newNormals;
  vtkFloatArray *newGradients;
  vtkPointData *pd;  
  vtkDataArray *inScalars;
  int dims[3], extent[6];
  vtkIdType estimatedSize;
  double spacing[3], origin[3];
  double bounds[6];
  int numContours=this->ContourValues->GetNumberOfContours();
  double *values=this->ContourValues->GetValues();

  vtkDebugMacro(<< "Executing marching cubes");

//
// Initialize and check input

//pd����
  pd=input->GetPointData();

  if (pd ==NULL)//ȷ���Ƿ�������
    {
    vtkErrorMacro(<<"PointData is NULL");
    return 1;
    }
  //inScalars
  inScalars=pd->GetScalars();
  if ( inScalars == NULL )
    {
    vtkErrorMacro(<<"Scalars must be defined for contouring");
    return 1;
    }

  if ( input->GetDataDimension() != 3 )
    {
    vtkErrorMacro(<<"Cannot contour data of dimension != 3");
    return 1;
    }

  input->GetDimensions(dims);
  input->GetOrigin(origin);
  input->GetSpacing(spacing);

  inInfo->Get(vtkStreamingDemandDrivenPipeline::WHOLE_EXTENT(), extent);

  // estimate the number of points from the volume dimensions
  // �����ά�ȹ��Ƶ������
  // dims = (maxX, maxY, maxZ)
  estimatedSize = static_cast<vtkIdType>(pow(1.0*dims[0]*dims[1]*dims[2], 0.75));
  estimatedSize = estimatedSize / 1024 * 1024; //multiple of 1024
  if (estimatedSize < 1024)
    {
    estimatedSize = 1024;
    }
  vtkDebugMacro(<< "Estimated allocation size is " << estimatedSize);
  newPts = vtkPoints::New(); 
  newPts->Allocate(estimatedSize,estimatedSize/2);
  // compute bounds for merging points
  // ����ϲ���ı߽�
  for ( int i=0; i<3; i++)
    {
    bounds[2*i] = origin[i] + extent[2*i] * spacing[i];
    bounds[2*i+1] = origin[i] + extent[2*i+1] * spacing[i];
    }
  if ( this->Locator == NULL )
    {
    this->CreateDefaultLocator();
    }
  this->Locator->InitPointInsertion (newPts, bounds, estimatedSize);

  if (this->ComputeNormals)
    {
    newNormals = vtkFloatArray::New();
    newNormals->SetNumberOfComponents(3);
    newNormals->Allocate(3*estimatedSize,3*estimatedSize/2);
    }
  else
    {
    newNormals = NULL;
    }

  if (this->ComputeGradients)
    {
    newGradients = vtkFloatArray::New();
    newGradients->SetNumberOfComponents(3);
    newGradients->Allocate(3*estimatedSize,3*estimatedSize/2);
    }
  else
    {
    newGradients = NULL;
    }

  newPolys = vtkCellArray::New();
  newPolys->Allocate(newPolys->EstimateSize(estimatedSize,3));

  if (this->ComputeScalars)
    {
    newScalars = vtkFloatArray::New();
    newScalars->Allocate(estimatedSize,estimatedSize/2);
    }
  else
    {
    newScalars = NULL;
    }

  if (inScalars->GetNumberOfComponents() == 1 )
    {
    void* scalars = inScalars->GetVoidPointer(0);
    switch (inScalars->GetDataType())
      {
      vtkTemplateMacro(
       vtkMarchingCubesSpaceComputeGradient(this, static_cast<VTK_TT*>(scalars),
                                        dims,origin,spacing,this->Locator,
                                        newScalars,newGradients,
                                        newNormals,newPolys,values,
                                        numContours)
        );
      } //switch
    }

  else //multiple components - have to convert
    {
    vtkIdType dataSize = static_cast<vtkIdType>(dims[0]) * dims[1] * dims[2];
    vtkDoubleArray *image=vtkDoubleArray::New();                          
    image->SetNumberOfComponents(inScalars->GetNumberOfComponents());    
    image->SetNumberOfTuples(image->GetNumberOfComponents()*dataSize);
    inScalars->GetTuples(0,dataSize,image);                               

    double *scalars = image->GetPointer(0);                              
    //this
	//scalars->
	//dims->
	//origin->
	//spacing->
	//this->Locator
	//newScalars->
	//newGradients->
	//newNormals->
	//newPolys->
	//values->
	//numContours->
	vtkMarchingCubesSpaceComputeGradient(this,scalars,dims,origin,spacing,this->Locator,
                  newScalars,newGradients,
                  newNormals,newPolys,values,numContours);
    image->Delete();
    }

  vtkDebugMacro(<<"Created: "
               << newPts->GetNumberOfPoints() << " points, "
               << newPolys->GetNumberOfCells() << " triangles");
  //
  // Update ourselves.  Because we don't know up front how many triangles
  // we've created, take care to reclaim memory.
  //
  output->SetPoints(newPts);
  newPts->Delete();

  output->SetPolys(newPolys);
  newPolys->Delete();

  if (newScalars)
    {
    int idx = output->GetPointData()->AddArray(newScalars);
    output->GetPointData()->SetActiveAttribute(idx, vtkDataSetAttributes::SCALARS);
    newScalars->Delete();
    }
  if (newGradients)
    {
    output->GetPointData()->SetVectors(newGradients);
    newGradients->Delete();
    }
  if (newNormals)
    {
    output->GetPointData()->SetNormals(newNormals);
    newNormals->Delete();
    }
  output->Squeeze();
  if (this->Locator)
    {
    this->Locator->Initialize(); //free storage
    }

  return 1;
}

// Description:
// Specify a spatial locator for merging points. By default,
// an instance of vtkMergePoints is used.
void vtkMarchingCubesSpace::SetLocator(vtkIncrementalPointLocator *locator)
{
  if ( this->Locator == locator )
    {
    return;
    }

  if ( this->Locator )
    {
    this->Locator->UnRegister(this);
    this->Locator = NULL;
    }

  if (locator)
    {
    locator->Register(this);
    }

  this->Locator = locator;
  this->Modified();
}

void vtkMarchingCubesSpace::CreateDefaultLocator()
{
  if ( this->Locator == NULL)
    {
    this->Locator = vtkMergePoints::New();
    }
}

// 1 input
/*intvtkMarchingCubesSpace::FillInputPortInformation(int, vtkInformation *info)
{
  info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkImageData");
  return 1;
}*/

// 2 inputs

//FillInputPortInformation
int vtkMarchingCubesSpace::FillInputPortInformation(int port, vtkInformation *info)
{
  if (port == 0)
    {
    info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkImageData");
    return 1;
    }
  else if (port == 1)
    {
    info->Set(vtkAlgorithm::INPUT_IS_REPEATABLE(), 1);
    info->Set(vtkAlgorithm::INPUT_IS_OPTIONAL(), 1);
    info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkImageData");
    return 1;
    }
  return 0;
}

//PrintSelf
void vtkMarchingCubesSpace::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);

  this->ContourValues->PrintSelf(os,indent.GetNextIndent());

  os << indent << "Compute Normals: " << (this->ComputeNormals ? "On\n" : "Off\n");
  os << indent << "Compute Gradients: " << (this->ComputeGradients ? "On\n" : "Off\n");
  os << indent << "Compute Scalars: " << (this->ComputeScalars ? "On\n" : "Off\n");

  if ( this->Locator )
    {
    os << indent << "Locator:" << this->Locator << "\n";
    this->Locator->PrintSelf(os,indent.GetNextIndent());
    }
  else
    {
    os << indent << "Locator: (none)\n";
    }
}


