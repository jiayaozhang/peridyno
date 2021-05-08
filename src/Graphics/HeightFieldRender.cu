#pragma once
#include "cuda_helper_math.h"
#include <glad/gl.h>
#include "HeightFieldRender.h"
#include "Topology/HeightField.h"
#include "Vector.h"
#include "Framework/Node.h"
#include "OpenGLContext.h"
#include "Color.h"


namespace dyno
{
	IMPLEMENT_CLASS(HeightFieldRenderModule)

	HeightFieldRenderModule::HeightFieldRenderModule()
		: VisualModule()
		, m_mode(HeightFieldRenderModule::Instance)
		, m_color(Vec3f(0.8, 0.8, 0.8))
	{
	}

	HeightFieldRenderModule::~HeightFieldRenderModule()
	{
	}

	bool HeightFieldRenderModule::initializeImpl()
	{
		Node* parent = getParent();
		if (parent == NULL)
		{
			Log::sendMessage(Log::Error, "Should insert this module into a node!");
			return false;
		}

		auto pSet = TypeInfo::cast<HeightField<DataType3f>>(parent->getTopologyModule());
		if (pSet == nullptr)
		{
			Log::sendMessage(Log::Error, "HeightFieldRenderModule: The topology module is not supported!");
			return false;
		}

		Log::sendMessage(Log::Info, "HeightFieldRenderModule successfully initialized!");

		return true;
	}

	__global__ void SetupTriangles(
		DArray<float3> vertices,
		DArray<float3> normals,
		DArray<float3> colors,
		DArray2D<float> heights,
		float dx,
		float dz,
		float3 origin,
		float3 color)
	{
		int i = threadIdx.x + blockIdx.x * blockDim.x;
		int j = threadIdx.y + blockIdx.y * blockDim.y;

		if (i < heights.nx() - 1 && j < heights.ny() - 1)
		{
			int id = i + j * heights.nx();

			//if (j == 2)
// 			{
// 				printf("%d \n", j);
// 			}

			float3 v1 = origin + make_float3(i*dx, heights(i, j), j*dz);
			float3 v2 = origin + make_float3((i + 1)*dx, heights(i + 1, j), j*dz);
			float3 v3 = origin + make_float3(i*dx, heights(i, j+1), (j+1)*dz);
			float3 v4 = origin + make_float3((i+1)*dx, heights(i+1, j+1), (j+1)*dz);

// 			float3 v1 = origin + make_float3(i*dx, 0.5f, j*dz);
// 			float3 v2 = origin + make_float3((i + 1)*dx, 0.5f, ((j + 1))*dz);
// 			float3 v3 = origin + make_float3(i*dx, 0.5f, (j + 1)*dz);
// 			float3 v4 = origin + make_float3((i + 1)*dx, 0.5f, (j + 1)*dz);

			vertices[3 * (2 * id) + 0] = v1;
			vertices[3 * (2 * id) + 1] = v2;
			vertices[3 * (2 * id) + 2] = v3;

			float3 triN1 = cross(v2 - v1, v3 - v1);
			triN1 = normalize(triN1);

			normals[3 * (2 * id) + 0] = triN1;
			normals[3 * (2 * id) + 1] = triN1;
			normals[3 * (2 * id) + 2] = triN1;

			colors[3 * (2 * id) + 0] = color;
			colors[3 * (2 * id) + 1] = color;
			colors[3 * (2 * id) + 2] = color;


			vertices[3 * (2 * id) + 3] = v3;
			vertices[3 * (2 * id) + 4] = v2;
			vertices[3 * (2 * id) + 5] = v4;

			float3 triN2 = cross(v2 - v3, v4 - v3);
			triN2 = normalize(triN2);

			normals[3 * (2 * id) + 3] = triN2;
			normals[3 * (2 * id) + 4] = triN2;
			normals[3 * (2 * id) + 5] = triN2;

			colors[3 * (2 * id) + 3] = color;
			colors[3 * (2 * id) + 4] = color;
			colors[3 * (2 * id) + 5] = color;
		}

		
	}

	void HeightFieldRenderModule::updateRenderingContext()
	{
		Node* parent = getParent();
		if (parent == NULL)
		{
			Log::sendMessage(Log::Error, "Should insert this module into a node!");
			return;
		}

		auto hf = TypeInfo::cast<HeightField<DataType3f>>(parent->getTopologyModule());
		if (hf == nullptr)
		{
			Log::sendMessage(Log::Error, "HeightFieldRenderModule: The topology module is not supported!");
			return;
		}


		auto heights = hf->getHeights();
		int numOfTriangles = (heights.nx() - 1)*(heights.ny() - 1) * 2;

		vertices.resize(3 * numOfTriangles);
		normals.resize(3 * numOfTriangles);
		colors.resize(3 * numOfTriangles);

		uint3 total_size;
		total_size.x = heights.nx() - 1;
		total_size.y = heights.ny() - 1;
		total_size.z = 1;

		auto ori = hf->getOrigin();

		cuExecute3D(total_size, SetupTriangles,
			vertices,
			normals,
			colors,
			heights,
			hf->getDx(),
			hf->getDz(),
			make_float3(ori[0], ori[1], ori[2]),
			make_float3(1.0, 0.0, 0.0));

		if (m_triangleRender == nullptr)
		{
			m_triangleRender = std::make_shared<TriangleRender>();
		}

		if (m_triangleRender->numberOfTrianlges() != numOfTriangles)
		{
			m_triangleRender->resize(numOfTriangles);
		}
		
		m_triangleRender->setVertexArray(vertices);
		m_triangleRender->setColorArray(colors);
		m_triangleRender->setNormalArray(normals);
	}

	void HeightFieldRenderModule::display()
	{
		glMatrixMode(GL_MODELVIEW_MATRIX);
		glPushMatrix();

		glRotatef(m_rotation.x, m_rotation.y, m_rotation.z, m_rotation.w);
		glTranslatef(m_translation[0], m_translation[1], m_translation[2]);
		glScalef(m_scale[0], m_scale[1], m_scale[2]);

		if (m_triangleRender != nullptr)
			m_triangleRender->display();

		glPopMatrix();
	}

	void HeightFieldRenderModule::setRenderMode(RenderMode mode)
	{
		m_mode = mode;
	}

	void HeightFieldRenderModule::setColor(Vec3f color)
	{
		m_color = color;
	}


	void HeightFieldRenderModule::setReferenceColor(float v)
	{
		m_refV = v;
	}

}