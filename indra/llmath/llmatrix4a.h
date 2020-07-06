/** 
 * @file llmatrix4a.h
 * @brief LLMatrix4a class header file - memory aligned and vectorized 4x4 matrix
 *
 * $LicenseInfo:firstyear=2007&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#ifndef	LL_LLMATRIX4A_H
#define	LL_LLMATRIX4A_H

#include "llvector4a.h"
#include "m4math.h"
#include "m3math.h"

LL_ALIGN_PREFIX(16)
class LLMatrix4a
{
private:
	LL_ALIGN_16(LLVector4a mMatrix[4]);
public:
	enum
	{
		ROW_FWD = 0,
		ROW_LEFT,
		ROW_UP,
		ROW_TRANS
	};

	void* operator new(size_t size)
	{
		return ll_aligned_malloc_16(size);
	}

	void* operator new[](size_t size)
	{
		return ll_aligned_malloc_16(size);
	}

	void operator delete(void* ptr)
	{
		ll_aligned_free_16(ptr);
	}

	void operator delete[](void* ptr)
	{
		ll_aligned_free_16(ptr);
	}

	LLMatrix4a() = default;
	LLMatrix4a(const LLQuad& q1,const LLQuad& q2,const LLQuad& q3,const LLQuad& q4)
	{
		mMatrix[0] = q1;
		mMatrix[1] = q2;
		mMatrix[2] = q3;
		mMatrix[3] = q4;
	}
	LLMatrix4a(const LLQuaternion2& quat)
	{
		const LLVector4a& xyzw = quat.getVector4a(); 
		LLVector4a nyxwz = _mm_shuffle_ps(xyzw, xyzw, _MM_SHUFFLE(2,3,0,1));
		nyxwz.negate();
	
		const LLVector4a xnyynx = _mm_unpacklo_ps(xyzw,nyxwz);
		const LLVector4a znwwnz = _mm_unpackhi_ps(xyzw,nyxwz);

		LLMatrix4a mata;
		mata.setRow<0>(_mm_shuffle_ps(xyzw, xnyynx, _MM_SHUFFLE(0,1,2,3)));
		mata.setRow<1>(_mm_shuffle_ps(znwwnz, xyzw, _MM_SHUFFLE(1,0,2,3)));
		mata.setRow<2>(_mm_shuffle_ps(xnyynx, xyzw, _MM_SHUFFLE(2,3,3,2)));
		mata.setRow<3>(_mm_shuffle_ps(xnyynx, znwwnz, _MM_SHUFFLE(2,3,1,3)));

		LLMatrix4a matb;
		matb.setRow<0>(_mm_shuffle_ps(xyzw, xnyynx, _MM_SHUFFLE(3,1,2,3)));
		matb.setRow<1>(_mm_shuffle_ps(znwwnz, xnyynx, _MM_SHUFFLE(1,0,2,3)));
		matb.setRow<2>(_mm_shuffle_ps(xnyynx, znwwnz, _MM_SHUFFLE(3,2,3,2)));
		matb.setRow<3>(xyzw);

		setMul(matb,mata);
	}

	inline F32* getF32ptr()
	{
		return mMatrix[0].getF32ptr();
	}
	
	inline const F32* getF32ptr() const
	{
		return mMatrix[0].getF32ptr();
	}

	inline void clear()
	{
		mMatrix[0].clear();
		mMatrix[1].clear();
		mMatrix[2].clear();
		mMatrix[3].clear();
	}

	inline void setIdentity()
	{
		static __m128 ones = _mm_set_ps(1.f,0.f,0.f,1.f);
		mMatrix[0] = _mm_movelh_ps(ones,_mm_setzero_ps());
		mMatrix[1] = _mm_movehl_ps(_mm_setzero_ps(),ones);
		mMatrix[2] = _mm_movelh_ps(_mm_setzero_ps(),ones);
		mMatrix[3] = _mm_movehl_ps(ones,_mm_setzero_ps());
	}

	inline void loadu(const LLMatrix4& src)
	{
		mMatrix[0].loadua(src.mMatrix[0]);
		mMatrix[1].loadua(src.mMatrix[1]);
		mMatrix[2].loadua(src.mMatrix[2]);
		mMatrix[3].loadua(src.mMatrix[3]);
	}

	inline void loadu(const LLMatrix3& src)
	{
		mMatrix[0].load3(src.mMatrix[0]);
		mMatrix[1].load3(src.mMatrix[1]);
		mMatrix[2].load3(src.mMatrix[2]);
		mMatrix[3].set(0,0,0,1.f);
	}

	inline void loadu(const F32* src)
	{
		mMatrix[0].loadua(src+0);
		mMatrix[1].loadua(src+4);
		mMatrix[2].loadua(src+8);
		mMatrix[3].loadua(src+12);
	}

	inline void add(const LLMatrix4a& rhs)
	{
		mMatrix[0].add(rhs.mMatrix[0]);
		mMatrix[1].add(rhs.mMatrix[1]);
		mMatrix[2].add(rhs.mMatrix[2]);
		mMatrix[3].add(rhs.mMatrix[3]);
	}

	inline void mul(const LLMatrix4a& rhs)
	{
		//Not using rotate4 to avoid extra copy of *this.
		LLVector4a x0,y0,z0,w0;
		LLVector4a x1,y1,z1,w1;
		LLVector4a x2,y2,z2,w2;
		LLVector4a x3,y3,z3,w3;

		//16 shuffles
		x0.splat<0>(rhs.mMatrix[0]);
		x1.splat<0>(rhs.mMatrix[1]);
		x2.splat<0>(rhs.mMatrix[2]);
		x3.splat<0>(rhs.mMatrix[3]);

		y0.splat<1>(rhs.mMatrix[0]);
		y1.splat<1>(rhs.mMatrix[1]);
		y2.splat<1>(rhs.mMatrix[2]);
		y3.splat<1>(rhs.mMatrix[3]);

		z0.splat<2>(rhs.mMatrix[0]);
		z1.splat<2>(rhs.mMatrix[1]);
		z2.splat<2>(rhs.mMatrix[2]);
		z3.splat<2>(rhs.mMatrix[3]);

		w0.splat<3>(rhs.mMatrix[0]);
		w1.splat<3>(rhs.mMatrix[1]);
		w2.splat<3>(rhs.mMatrix[2]);
		w3.splat<3>(rhs.mMatrix[3]);

		//16 muls
		x0.mul(mMatrix[0]);
		x1.mul(mMatrix[0]);
		x2.mul(mMatrix[0]);
		x3.mul(mMatrix[0]);

		y0.mul(mMatrix[1]);
		y1.mul(mMatrix[1]);
		y2.mul(mMatrix[1]);
		y3.mul(mMatrix[1]);

		z0.mul(mMatrix[2]);
		z1.mul(mMatrix[2]);
		z2.mul(mMatrix[2]);
		z3.mul(mMatrix[2]);

		w0.mul(mMatrix[3]);
		w1.mul(mMatrix[3]);
		w2.mul(mMatrix[3]);
		w3.mul(mMatrix[3]);

		//12 adds
		x0.add(y0);
		z0.add(w0);

		x1.add(y1);
		z1.add(w1);

		x2.add(y2);
		z2.add(w2);

		x3.add(y3);
		z3.add(w3);

		mMatrix[0].setAdd(x0,z0);
		mMatrix[1].setAdd(x1,z1);
		mMatrix[2].setAdd(x2,z2);
		mMatrix[3].setAdd(x3,z3);
	}

	inline void setRows(const LLVector4a& r0, const LLVector4a& r1, const LLVector4a& r2)
	{
		mMatrix[0] = r0;
		mMatrix[1] = r1;
		mMatrix[2] = r2;
	}

	template<int N>
	inline void setRow(const LLVector4a& row)
	{
		mMatrix[N] = row;
	}

	template<int N>
	inline const LLVector4a& getRow() const
	{
		return mMatrix[N];
	}

	template<int N>
	inline LLVector4a& getRow()
	{
		return mMatrix[N];
	}

	template<int N>
	inline void setColumn(const LLVector4a& col)
	{
		mMatrix[0].copyComponent<N>(col.getScalarAt<0>());
		mMatrix[1].copyComponent<N>(col.getScalarAt<1>());
		mMatrix[2].copyComponent<N>(col.getScalarAt<2>());
		mMatrix[3].copyComponent<N>(col.getScalarAt<3>());
	}

	template<int N>
	inline LLVector4a getColumn()
	{
		LLVector4a v;
		v.copyComponent<0>(mMatrix[0].getScalarAt<N>());
		v.copyComponent<1>(mMatrix[1].getScalarAt<N>());
		v.copyComponent<2>(mMatrix[2].getScalarAt<N>());
		v.copyComponent<3>(mMatrix[3].getScalarAt<N>());
		return v;
	}

	inline void setMul(const LLMatrix4a& m, const F32 s)
	{
		const LLVector4a ssss(s);
		mMatrix[0].setMul(m.mMatrix[0], ssss);
		mMatrix[1].setMul(m.mMatrix[1], ssss);
		mMatrix[2].setMul(m.mMatrix[2], ssss);
		mMatrix[3].setMul(m.mMatrix[3], ssss);
	}

	inline void setMul(const LLMatrix4a& m0, const LLMatrix4a& m1)
	{
		m0.rotate4(m1.mMatrix[0],mMatrix[0]);
		m0.rotate4(m1.mMatrix[1],mMatrix[1]);
		m0.rotate4(m1.mMatrix[2],mMatrix[2]);
		m0.rotate4(m1.mMatrix[3],mMatrix[3]);
	}

	inline void setLerp(const LLMatrix4a& a, const LLMatrix4a& b, F32 w)
	{
		LLVector4a d0,d1,d2,d3;
		d0.setSub(b.mMatrix[0], a.mMatrix[0]);
		d1.setSub(b.mMatrix[1], a.mMatrix[1]);
		d2.setSub(b.mMatrix[2], a.mMatrix[2]);
		d3.setSub(b.mMatrix[3], a.mMatrix[3]);

		// this = a + d*w
		
		const LLVector4a wwww(w);
		d0.mul(wwww);
		d1.mul(wwww);
		d2.mul(wwww);
		d3.mul(wwww);

		mMatrix[0].setAdd(a.mMatrix[0],d0);
		mMatrix[1].setAdd(a.mMatrix[1],d1);
		mMatrix[2].setAdd(a.mMatrix[2],d2);
		mMatrix[3].setAdd(a.mMatrix[3],d3);
	}
	
	//Singu Note: Don't mess with this. It's intentionally different from LL's. 
	// Note how res isn't manipulated until the very end.
	//Fast(er). Treats v[VW] as 0.f
	inline void rotate(const LLVector4a& v, LLVector4a& res) const
	{
		LLVector4a x,y,z;

		x.splat<0>(v);
		y.splat<1>(v);
		z.splat<2>(v);

		x.mul(mMatrix[0]);
		y.mul(mMatrix[1]);
		z.mul(mMatrix[2]);

		x.add(y);
		res.setAdd(x,z);
	}
	
	//Proper. v[VW] as v[VW]
	inline void rotate4(const LLVector4a& v, LLVector4a& res) const
	{
		LLVector4a x,y,z,w;

		x.splat<0>(v);
		y.splat<1>(v);
		z.splat<2>(v);
		w.splat<3>(v);

		x.mul(mMatrix[0]);
		y.mul(mMatrix[1]);
		z.mul(mMatrix[2]);
		w.mul(mMatrix[3]);

		x.add(y);
		z.add(w);
		res.setAdd(x,z);
	}

	//Fast(er). Treats v[VW] as 1.f
	inline void affineTransform(const LLVector4a& v, LLVector4a& res) const
	{
		LLVector4a x,y,z;

		x.splat<0>(v);
		y.splat<1>(v);
		z.splat<2>(v);

		x.mul(mMatrix[0]);
		y.mul(mMatrix[1]);
		z.mul(mMatrix[2]);

		x.add(y);
		z.add(mMatrix[3]);
		res.setAdd(x,z);
	}

	inline void perspectiveTransform(const LLVector4a& v, LLVector4a& res) const
	{
		LLVector4a x,y,z,s,t,p,q;

		x.splat<0>(v);
		y.splat<1>(v);
		z.splat<2>(v);

		s.splat<3>(mMatrix[0]);
		t.splat<3>(mMatrix[1]);
		p.splat<3>(mMatrix[2]);
		q.splat<3>(mMatrix[3]);

		s.mul(x);
		t.mul(y);
		p.mul(z);
		q.add(s);
		t.add(p);
		q.add(t);

		x.mul(mMatrix[0]);
		y.mul(mMatrix[1]);
		z.mul(mMatrix[2]);

		x.add(y);
		z.add(mMatrix[3]);
		res.setAdd(x,z);
		res.div(q);
	}

	inline void transpose()
	{
		__m128 q1 = _mm_unpackhi_ps(mMatrix[0],mMatrix[1]);
		__m128 q2 = _mm_unpacklo_ps(mMatrix[0],mMatrix[1]);
		__m128 q3 = _mm_unpacklo_ps(mMatrix[2],mMatrix[3]);
		__m128 q4 = _mm_unpackhi_ps(mMatrix[2],mMatrix[3]);

		mMatrix[0] = _mm_movelh_ps(q2,q3);
		mMatrix[1] = _mm_movehl_ps(q3,q2);
		mMatrix[2] = _mm_movelh_ps(q1,q4);
		mMatrix[3] = _mm_movehl_ps(q4,q1);
	}

//  Following procedure adapted from:
//		http://software.intel.com/en-us/articles/optimized-matrix-library-for-use-with-the-intel-pentiumr-4-processors-sse2-instructions/
//
//  License/Copyright Statement:
//		
//			Copyright (c) 2001 Intel Corporation.
//
//		Permition is granted to use, copy, distribute and prepare derivative works 
//		of this library for any purpose and without fee, provided, that the above 
//		copyright notice and this statement appear in all copies.  
//		Intel makes no representations about the suitability of this library for 
//		any purpose, and specifically disclaims all warranties. 
//		See LEGAL-intel_matrixlib.TXT for all the legal information.
	inline float invert()
	{
		LL_ALIGN_16(const unsigned int Sign_PNNP[4]) = { 0x00000000, 0x80000000, 0x80000000, 0x00000000 };

		// The inverse is calculated using "Divide and Conquer" technique. The 
		// original matrix is divide into four 2x2 sub-matrices. Since each 
		// register holds four matrix element, the smaller matrices are 
		// represented as a registers. Hence we get a better locality of the 
		// calculations.

		LLVector4a A = _mm_movelh_ps(mMatrix[0], mMatrix[1]),    // the four sub-matrices 
				B = _mm_movehl_ps(mMatrix[1], mMatrix[0]),
				C = _mm_movelh_ps(mMatrix[2], mMatrix[3]),
				D = _mm_movehl_ps(mMatrix[3], mMatrix[2]);
		LLVector4a iA, iB, iC, iD,					// partial inverse of the sub-matrices
				DC, AB;
		LLSimdScalar dA, dB, dC, dD;                 // determinant of the sub-matrices
		LLSimdScalar det, d, d1, d2;
		LLVector4a rd;

		//  AB = A# * B
		AB.setMul(_mm_shuffle_ps(A,A,0x0F), B);
		AB.sub(_mm_mul_ps(_mm_shuffle_ps(A,A,0xA5), _mm_shuffle_ps(B,B,0x4E)));
		//  DC = D# * C
		DC.setMul(_mm_shuffle_ps(D,D,0x0F), C);
		DC.sub(_mm_mul_ps(_mm_shuffle_ps(D,D,0xA5), _mm_shuffle_ps(C,C,0x4E)));

		//  dA = |A|
		dA = _mm_mul_ps(_mm_shuffle_ps(A, A, 0x5F),A);
		dA -= _mm_movehl_ps(dA,dA);
		//  dB = |B|
		dB = _mm_mul_ps(_mm_shuffle_ps(B, B, 0x5F),B);
		dB -= _mm_movehl_ps(dB,dB);

		//  dC = |C|
		dC = _mm_mul_ps(_mm_shuffle_ps(C, C, 0x5F),C);
		dC -= _mm_movehl_ps(dC,dC);
		//  dD = |D|
		dD = _mm_mul_ps(_mm_shuffle_ps(D, D, 0x5F),D);
		dD -= _mm_movehl_ps(dD,dD);

		//  d = trace(AB*DC) = trace(A#*B*D#*C)
		d = _mm_mul_ps(_mm_shuffle_ps(DC,DC,0xD8),AB);

		//  iD = C*A#*B
		iD.setMul(_mm_shuffle_ps(C,C,0xA0), _mm_movelh_ps(AB,AB));
		iD.add(_mm_mul_ps(_mm_shuffle_ps(C,C,0xF5), _mm_movehl_ps(AB,AB)));
		//  iA = B*D#*C
		iA.setMul(_mm_shuffle_ps(B,B,0xA0), _mm_movelh_ps(DC,DC));
		iA.add(_mm_mul_ps(_mm_shuffle_ps(B,B,0xF5), _mm_movehl_ps(DC,DC)));

		//  d = trace(AB*DC) = trace(A#*B*D#*C) [continue]
		d = _mm_add_ps(d, _mm_movehl_ps(d, d));
		d += _mm_shuffle_ps(d, d, 1);
		d1 = dA*dD;
		d2 = dB*dC;

		//  iD = D*|A| - C*A#*B
		iD.setSub(_mm_mul_ps(D,_mm_shuffle_ps(dA,dA,0)), iD);

		//  iA = A*|D| - B*D#*C;
		iA.setSub(_mm_mul_ps(A,_mm_shuffle_ps(dD,dD,0)), iA);

		//  det = |A|*|D| + |B|*|C| - trace(A#*B*D#*C)
		det = d1+d2-d;

		__m128 is_zero_mask = _mm_cmpeq_ps(det,_mm_setzero_ps());
		rd = _mm_div_ss(_mm_set_ss(1.f),_mm_or_ps(_mm_andnot_ps(is_zero_mask, det), _mm_and_ps(is_zero_mask, _mm_set_ss(1.f))));
#ifdef ZERO_SINGULAR
		rd = _mm_and_ps(_mm_cmpneq_ss(det,_mm_setzero_ps()), rd);
#endif

		//  iB = D * (A#B)# = D*B#*A
		iB.setMul(D, _mm_shuffle_ps(AB,AB,0x33));
		iB.sub(_mm_mul_ps(_mm_shuffle_ps(D,D,0xB1), _mm_shuffle_ps(AB,AB,0x66)));
		//  iC = A * (D#C)# = A*C#*D
		iC.setMul(A, _mm_shuffle_ps(DC,DC,0x33));
		iC.sub(_mm_mul_ps(_mm_shuffle_ps(A,A,0xB1), _mm_shuffle_ps(DC,DC,0x66)));

		rd = _mm_shuffle_ps(rd,rd,0);
		rd = _mm_xor_ps(rd, _mm_load_ps((const float*)Sign_PNNP));

		//  iB = C*|B| - D*B#*A
		iB.setSub(_mm_mul_ps(C,_mm_shuffle_ps(dB,dB,0)), iB);

		//  iC = B*|C| - A*C#*D;
		iC.setSub(_mm_mul_ps(B,_mm_shuffle_ps(dC,dC,0)), iC);


		//  iX = iX / det
		iA.mul(rd);
		iB.mul(rd);
		iC.mul(rd);
		iD.mul(rd);

		mMatrix[0] = _mm_shuffle_ps(iA,iB,0x77);
		mMatrix[1] = _mm_shuffle_ps(iA,iB,0x22);
		mMatrix[2] = _mm_shuffle_ps(iC,iD,0x77);
		mMatrix[3] = _mm_shuffle_ps(iC,iD,0x22);
		
		F32 ret;
		_mm_store_ss(&ret,det);
		return ret;
	}

	//=============Affine transformation matrix only=========================

	//Multiply matrix with a pure translation matrix.
	inline void applyTranslation_affine(const F32& x, const F32& y, const F32& z)
	{
		const LLVector4a xyz0(x,y,z,0);	//load
		LLVector4a xxxx;
		xxxx.splat<0>(xyz0);
		LLVector4a yyyy;
		yyyy.splat<1>(xyz0);
		LLVector4a zzzz;
		zzzz.splat<2>(xyz0);

		LLVector4a sum1;
		LLVector4a sum2;
		LLVector4a sum3;

		sum1.setMul(xxxx,mMatrix[0]);
		sum2.setMul(yyyy,mMatrix[1]);
		sum3.setMul(zzzz,mMatrix[2]);

		mMatrix[3].add(sum1);
		mMatrix[3].add(sum2);
		mMatrix[3].add(sum3);
	}

	//Multiply matrix with a pure translation matrix.
	inline void applyTranslation_affine(const LLVector3& trans)
	{
		applyTranslation_affine(trans.mV[VX],trans.mV[VY],trans.mV[VZ]);
	}

	//Multiply matrix with a pure scale matrix.
	inline void applyScale_affine(const F32& x, const F32& y, const F32& z)
	{
		const LLVector4a xyz0(x,y,z,0);	//load
		LLVector4a xxxx;
		xxxx.splat<0>(xyz0);
		LLVector4a yyyy;
		yyyy.splat<1>(xyz0);
		LLVector4a zzzz;
		zzzz.splat<2>(xyz0);

		mMatrix[0].mul(xxxx);
		mMatrix[1].mul(yyyy);
		mMatrix[2].mul(zzzz);
	}

	//Multiply matrix with a pure scale matrix.
	inline void applyScale_affine(const LLVector3& scale)
	{
		applyScale_affine(scale.mV[VX],scale.mV[VY],scale.mV[VZ]);
	}

	//Multiply matrix with a pure scale matrix.
	inline void applyScale_affine(const F32& s)
	{
		const LLVector4a scale(s);	//load
		mMatrix[0].mul(scale);
		mMatrix[1].mul(scale);
		mMatrix[2].mul(scale);
	}

	//Direct addition to row3.
	inline void translate_affine(const LLVector3& trans)
	{
		LLVector4a translation;
		translation.load3(trans.mV);
		mMatrix[3].add(translation);
	}

	//Direct assignment of row3.
	inline void setTranslate_affine(const LLVector3& trans)
	{
		static const LLVector4Logical mask = _mm_load_ps((F32*)&S_V4LOGICAL_MASK_TABLE[3*4]);

		LLVector4a translation;
		translation.load3(trans.mV);
		
		mMatrix[3].setSelectWithMask(mask,mMatrix[3],translation);
	}

	inline void mul_affine(const LLMatrix4a& rhs)
	{
		LLVector4a x0,y0,z0;
		LLVector4a x1,y1,z1;
		LLVector4a x2,y2,z2;
		LLVector4a x3,y3,z3;

		//12 shuffles
		x0.splat<0>(rhs.mMatrix[0]);
		x1.splat<0>(rhs.mMatrix[1]);
		x2.splat<0>(rhs.mMatrix[2]);
		x3.splat<0>(rhs.mMatrix[3]);

		y0.splat<1>(rhs.mMatrix[0]);
		y1.splat<1>(rhs.mMatrix[1]);
		y2.splat<1>(rhs.mMatrix[2]);
		y3.splat<1>(rhs.mMatrix[3]);

		z0.splat<2>(rhs.mMatrix[0]);
		z1.splat<2>(rhs.mMatrix[1]);
		z2.splat<2>(rhs.mMatrix[2]);
		z3.splat<2>(rhs.mMatrix[3]);

		//12 muls
		x0.mul(mMatrix[0]);
		x1.mul(mMatrix[0]);
		x2.mul(mMatrix[0]);
		x3.mul(mMatrix[0]);

		y0.mul(mMatrix[1]);
		y1.mul(mMatrix[1]);
		y2.mul(mMatrix[1]);
		y3.mul(mMatrix[1]);

		z0.mul(mMatrix[2]);
		z1.mul(mMatrix[2]);
		z2.mul(mMatrix[2]);
		z3.mul(mMatrix[2]);

		//9 adds
		x0.add(y0);

		x1.add(y1);

		x2.add(y2);

		x3.add(y3);
		z3.add(mMatrix[3]);

		mMatrix[0].setAdd(x0,z0);
		mMatrix[1].setAdd(x1,z1);
		mMatrix[2].setAdd(x2,z2);
		mMatrix[3].setAdd(x3,z3);
	}

	inline void extractRotation_affine()
	{
		static const LLVector4Logical mask = _mm_load_ps((F32*)&S_V4LOGICAL_MASK_TABLE[3*4]);
		mMatrix[0].setSelectWithMask(mask,_mm_setzero_ps(),mMatrix[0]);
		mMatrix[1].setSelectWithMask(mask,_mm_setzero_ps(),mMatrix[1]);
		mMatrix[2].setSelectWithMask(mask,_mm_setzero_ps(),mMatrix[2]);
		mMatrix[3].setSelectWithMask(mask,LLVector4a(1.f),_mm_setzero_ps());
	}

	//======================Logic====================
private:
	template<bool mins> inline void init_foos(LLMatrix4a& foos) const
	{
		static bool done(false);
		if (done) return;
		const LLVector4a delta(0.0001f);
		foos.setIdentity();
		foos.getRow<0>().sub(delta);
		foos.getRow<1>().sub(delta);
		foos.getRow<2>().sub(delta);
		foos.getRow<3>().sub(delta);
		done = true;
	}

public:
	inline bool isIdentity() const
	{
		static LLMatrix4a mins;
		static LLMatrix4a maxs;

		init_foos<false>(mins);
		init_foos<true>(maxs);

		LLVector4a mask1 = _mm_and_ps(_mm_cmpgt_ps(mMatrix[0],mins.getRow<0>()), _mm_cmplt_ps(mMatrix[0],maxs.getRow<0>()));
		LLVector4a mask2 = _mm_and_ps(_mm_cmpgt_ps(mMatrix[1],mins.getRow<1>()), _mm_cmplt_ps(mMatrix[1],maxs.getRow<1>()));
		LLVector4a mask3 = _mm_and_ps(_mm_cmpgt_ps(mMatrix[2],mins.getRow<2>()), _mm_cmplt_ps(mMatrix[2],maxs.getRow<2>()));
		LLVector4a mask4 = _mm_and_ps(_mm_cmpgt_ps(mMatrix[3],mins.getRow<3>()), _mm_cmplt_ps(mMatrix[3],maxs.getRow<3>()));

		mask1 = _mm_and_ps(mask1,mask2);
		mask2 = _mm_and_ps(mask3,mask4);

		return _mm_movemask_epi8(_mm_castps_si128(_mm_and_ps(mask1, mask2))) == 0xFFFF;
	}
} LL_ALIGN_POSTFIX(16);


inline std::ostream& operator<<(std::ostream& s, const LLMatrix4a& m)
{
    s << "[" << m.getF32ptr()[0] << ", " << m.getF32ptr()[1] << ", " << m.getF32ptr()[2] << ", " << m.getF32ptr()[3] << "]";
    return s;
} 

void matMulBoundBox(const LLMatrix4a &a, const LLVector4a *in_extents, LLVector4a *out_extents);

#if !defined(LL_DEBUG)
static_assert(std::is_trivial<LLMatrix4a>::value, "LLMatrix4a must be a trivial type");
static_assert(std::is_standard_layout<LLMatrix4a>::value, "LLMatrix4a must be a standard layout type");
#endif
#endif
