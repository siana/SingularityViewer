#include "llfloater.h"

class LLIconCtrl;

class SHFloaterMediaTicker : public LLFloater, public LLSingleton<SHFloaterMediaTicker>
{
	friend class LLSingleton<SHFloaterMediaTicker>;
public:
	SHFloaterMediaTicker();	//ctor

	virtual ~SHFloaterMediaTicker() {}
	/*virtual*/ BOOL postBuild();
	/*virtual*/ void draw();
	/*virtual*/ void onOpen();
	/*virtual*/ void onClose(bool app_quitting);
	/*virtual*/ void reshape(S32 width, S32 height, BOOL called_from_parent = TRUE);
	static void showInstance();	//use to create.
private:
	void updateTickerText(); //called via draw.
	void drawOscilloscope(); //called via draw.
	bool setPaused(bool pause); //returns true on state change.
	void resetTicker(); //Resets tickers to their innitial values (no offset).
	bool setArtist(const std::string &artist);	//returns true on change
	bool setTitle(const std::string &title);	//returns true on change
	S32 countExtraChars(LLTextBox *texbox, const std::string &text);	//calculates how many characters are truncated by bounds.
	void iterateTickerOffset();	//Logic that actually shuffles the text to the left.

	enum ePlayState
	{
		STATE_PAUSED,
		STATE_PLAYING
	};

	ePlayState mPlayState;
	std::string mszLoading;
	std::string mszPaused;
	std::string mszArtist;
	std::string mszTitle;
	LLTimer mScrollTimer;
	LLTimer mLoadTimer;
	LLTimer mUpdateTimer;
	S32 mArtistScrollChars;
	S32 mTitleScrollChars;
	S32 mCurScrollChar;

	LLColor4 mOscillatorColor;

	//UI elements
	LLIconCtrl* mTickerBackground;
	LLTextBox* mArtistText;
	LLTextBox* mTitleText;
	LLUICtrl* mVisualizer;
};

//Menu callbacks.
BOOL handle_ticker_enabled(void *);
BOOL handle_ticker_check(void *);
void handle_ticker_toggle(void *);

