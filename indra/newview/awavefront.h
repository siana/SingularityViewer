/**
 * @file awavefront.h
 * @brief A system which allows saving in-world objects to Wavefront .OBJ files for offline texturizing/shading.
 * @authors Apelsin, Lirusaito
 *
 * $LicenseInfo:firstyear=2011&license=LGPLV3$
 * Copyright (C) 2011-2013 Apelsin
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA */

#ifndef AWAVEFRONT
#define AWAVEFRONT

class LLFace;
class LLPolyMesh;
class LLViewerObject;
class LLVOAvatar;

typedef std::vector<std::pair<LLVector3, LLVector2> > vert_t;
typedef std::vector<LLVector3> vec3_t;

struct tri
{
	tri(int a, int b, int c) : v0(a), v1(b), v2(c) {}
	int v0;
	int v1;
	int v2;
};
typedef std::vector<tri> tri_t;

class Wavefront
{
public:
	vert_t vertices;
	vec3_t normals; //null unless otherwise specified!
	tri_t triangles; //because almost all surfaces in SL are triangles
	std::string name;
	Wavefront(vert_t v, tri_t t);
	Wavefront(const LLVolumeFace* face, const LLXform* transform = NULL, const LLXform* transform_normals = NULL);
	Wavefront(LLFace* face, LLPolyMesh* mesh = NULL, const LLXform* transform = NULL, const LLXform* transform_normals = NULL);
	static void Transform(vert_t& v, const LLXform* x); //helper function
	static void Transform(vec3_t& v, const LLXform* x); //helper function
};

class WavefrontSaver
{
public:
	std::vector<Wavefront> obj_v;
	LLVector3 offset;
	WavefrontSaver();
	void Add(const Wavefront& obj);
	void Add(const LLVolume* vol, const LLXform* transform = NULL, const LLXform* transform_normals = NULL);
	void Add(const LLViewerObject* some_vo);
	void Add(const LLVOAvatar* av_vo);
	bool saveFile(LLFILE* fp);
};

#endif // AWAVEFRONT

