/**
 * Copyright 2024 Xiaowei He
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
#include "PointSet.h"
#include "EdgeSet.h"
#include "TriangleSet.h"

namespace dyno
{
	template<typename TDataType>
	class PolygonSet : public PointSet<TDataType>
	{
		typedef typename TDataType::Real Real;
		typedef typename TDataType::Coord Coord;

		typedef typename TopologyModule::Edge Edge;
		typedef typename TopologyModule::Triangle Triangle;

	public:
		PolygonSet();
		~PolygonSet() override;

		/**
		 * @brief initialize polygon indices
		 */
		void setPolygons(const CArrayList<uint>& indices);
		void setPolygons(const DArrayList<uint>& indices);

		void copyFrom(PolygonSet<TDataType>& polygons);

		bool isEmpty() override;

		/**
		 * @brief extract and merge edges from all polygons into one EdgeSet
		 */
		void extractEdgeSet(EdgeSet<TDataType>& es);

		/**
		 * @brief turn all polygons into triangles and store into one TriangleSet
		 */
		void turnIntoTriangleSet(TriangleSet<TDataType>& ts);

	protected:
		void updateTopology() override;

	private:
		DArrayList<uint> mPolygonIndex;
	};
}
