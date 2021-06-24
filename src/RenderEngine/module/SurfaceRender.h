/**
 * Copyright 2017-2021 Jian SHI
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "GLVisualModule.h"
#include "GLVertexArray.h"
#include "GLShader.h"

namespace dyno
{
	class SurfaceRenderer : public GLVisualModule
	{
		DECLARE_CLASS(SurfaceRenderer)
	public:
		SurfaceRenderer();

	protected:
		virtual void paintGL(RenderMode mode) override;
		virtual void updateGL() override;
		virtual bool initializeGL() override;

	private:

		GLShaderProgram mShaderProgram;

		GLCudaBuffer	mVertexBuffer;
		GLCudaBuffer 	mIndexBuffer;
		GLVertexArray	mVAO;

		unsigned int	mDrawCount = 0;
	};
};