#ifndef __HIPPO_LIMITS_H__
#define __HIPPO_LIMITS_H__


class HippoLimits
{
	LOG_CLASS(HippoLimits);
public:
	HippoLimits();

	float getMaxHeight()      const { return mMaxHeight;      }
	float getMinHoleSize()    const { return mMinHoleSize;    }
	float getMaxHollow()      const { return mMaxHollow;      }
	float getMaxPrimScale()   const { return mMaxPrimScale;   }
	float getMinPrimScale()   const { return mMinPrimScale;   }

	void setLimits();

private:
	float mMaxHeight;
	float mMinHoleSize;
	float mMaxHollow;
	float mMaxPrimScale;
	float mMinPrimScale;

	void setOpenSimLimits();
	void setWhiteCoreLimits();
	void setSecondLifeLimits();
};


extern HippoLimits *gHippoLimits;


#endif
