/*
Minetest
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "test.h"

#include "gamedef.h"
#include "voxelalgorithms.h"
#include "util/numeric.h"
#include "dummymap.h"

class TestVoxelAlgorithms : public TestBase {
public:
	TestVoxelAlgorithms() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestVoxelAlgorithms"; }

	void runTests(IGameDef *gamedef);

	void testVoxelLineIterator();
	void testLighting(IGameDef *gamedef);
};

static TestVoxelAlgorithms g_test_instance;

void TestVoxelAlgorithms::runTests(IGameDef *gamedef)
{
	TEST(testVoxelLineIterator);
	TEST(testLighting, gamedef);
}

////////////////////////////////////////////////////////////////////////////////

void TestVoxelAlgorithms::testVoxelLineIterator()
{
	// Test some lines
	// Do not test lines that start or end on the border of
	// two voxels as rounding errors can make the test fail!
	std::vector<core::line3d<f32> > lines;
	for (f32 x = -9.1; x < 9; x += 3.124) {
	for (f32 y = -9.2; y < 9; y += 3.123) {
	for (f32 z = -9.3; z < 9; z += 3.122) {
		lines.emplace_back(-x, -y, -z, x, y, z);
	}
	}
	}
	lines.emplace_back(0, 0, 0, 0, 0, 0);
	// Test every line
	std::vector<core::line3d<f32> >::iterator it = lines.begin();
	for (; it < lines.end(); it++) {
		core::line3d<f32> l = *it;

		// Initialize test
		voxalgo::VoxelLineIterator iterator(l.start, l.getVector());

		//Test the first voxel
		v3s16 start_voxel = floatToInt(l.start, 1);
		UASSERT(iterator.m_current_node_pos == start_voxel);

		// Values for testing
		v3s16 end_voxel = floatToInt(l.end, 1);
		v3s16 voxel_vector = end_voxel - start_voxel;
		int nodecount = abs(voxel_vector.X) + abs(voxel_vector.Y)
			+ abs(voxel_vector.Z);
		int actual_nodecount = 0;
		v3s16 old_voxel = iterator.m_current_node_pos;

		while (iterator.hasNext()) {
			iterator.next();
			actual_nodecount++;
			v3s16 new_voxel = iterator.m_current_node_pos;
			// This must be a neighbor of the old voxel
			UASSERTEQ(f32, (new_voxel - old_voxel).getLengthSQ(), 1);
			// The line must intersect with the voxel
			v3f voxel_center = intToFloat(iterator.m_current_node_pos, 1);
			aabb3f box(voxel_center - v3f(0.5, 0.5, 0.5),
				voxel_center + v3f(0.5, 0.5, 0.5));
			UASSERT(box.intersectsWithLine(l));
			// Update old voxel
			old_voxel = new_voxel;
		}

		// Test last node
		UASSERT(iterator.m_current_node_pos == end_voxel);
		// Test node count
		UASSERTEQ(int, actual_nodecount, nodecount);
	}
}

void TestVoxelAlgorithms::testLighting(IGameDef *gamedef)
{
	v3s16 pmin(-32, -32, -32);
	v3s16 pmax(31, 31, 31);
	v3s16 bpmin = getNodeBlockPos(pmin), bpmax = getNodeBlockPos(pmax);
	DummyMap map(gamedef, bpmin, bpmax);

	// Make a 21x21x21 hollow box centered at the origin.
	{
		std::map<v3s16, MapBlock*> modified_blocks;
		MMVManip vm(&map);
		vm.initialEmerge(bpmin, bpmax, false);
		s32 volume = vm.m_area.getVolume();
		for (s32 i = 0; i < volume; i++)
			vm.m_data[i] = MapNode(CONTENT_AIR);
		for (s16 z = -10; z <= 10; z++)
		for (s16 y = -10; y <= 10; y++)
		for (s16 x = -10; x <= 10; x++)
			vm.setNodeNoEmerge(v3s16(x, y, z), MapNode(t_CONTENT_STONE));
		for (s16 z = -9; z <= 9; z++)
		for (s16 y = -9; y <= 9; y++)
		for (s16 x = -9; x <= 9; x++)
			vm.setNodeNoEmerge(v3s16(x, y, z), MapNode(CONTENT_AIR));
		voxalgo::blit_back_with_light(&map, &vm, &modified_blocks);
	}

	// Place two holes on the edges a torch in the center.
	{
		std::map<v3s16, MapBlock*> modified_blocks;
		map.addNodeAndUpdate(v3s16(-10, 0, 0), MapNode(CONTENT_AIR), modified_blocks);
		map.addNodeAndUpdate(v3s16(9, 10, -9), MapNode(t_CONTENT_WATER), modified_blocks);
		map.addNodeAndUpdate(v3s16(0, 0, 0), MapNode(t_CONTENT_TORCH), modified_blocks);
	}

	const NodeDefManager *ndef = gamedef->ndef();
	{
		MapNode n = map.getNode(v3s16(9, 9, -9));
		UASSERTEQ(int, n.getLight(LIGHTBANK_NIGHT, ndef), 0);
		UASSERTEQ(int, n.getLight(LIGHTBANK_DAY, ndef), 13);
	}
	{
		MapNode n = map.getNode(v3s16(0, 1, 0));
		UASSERTEQ(int, n.getLight(LIGHTBANK_NIGHT, ndef), 12);
		UASSERTEQ(int, n.getLight(LIGHTBANK_DAY, ndef), 12);
	}
	{
		MapNode n = map.getNode(v3s16(-9, -1, 0));
		UASSERTEQ(int, n.getLight(LIGHTBANK_NIGHT, ndef), 3);
		UASSERTEQ(int, n.getLight(LIGHTBANK_DAY, ndef), 12);
	}
	{
		MapNode n = map.getNode(v3s16(-10, 0, 0));
		UASSERTEQ(int, n.getLight(LIGHTBANK_NIGHT, ndef), 3);
		UASSERTEQ(int, n.getLight(LIGHTBANK_DAY, ndef), 14);
	}
	{
		MapNode n = map.getNode(v3s16(-11, 0, 0));
		UASSERTEQ(int, n.getLight(LIGHTBANK_NIGHT, ndef), 2);
		UASSERTEQ(int, n.getLight(LIGHTBANK_DAY, ndef), 15);
	}
}
