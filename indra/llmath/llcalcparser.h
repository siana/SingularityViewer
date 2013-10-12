/*
 *  LLCalcParser.h
 *  Copyright 2008 Aimee Walton.
 * $LicenseInfo:firstyear=2008&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2008, Linden Research, Inc.
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
 *
 */

#ifndef LL_CALCPARSER_H
#define LL_CALCPARSER_H

#include <boost/spirit/version.hpp>
#if !defined(SPIRIT_VERSION) || SPIRIT_VERSION < 0x2010
#error "At least Spirit version 2.1 required"
#endif

// Add this in if we want boost math constants.
//#include <boost/math/constants/constants.hpp>
#include <boost/spirit/include/phoenix.hpp>
#include <boost/spirit/include/qi.hpp>

namespace expression {


//TODO: If we can find a better way to do this with boost::pheonix::bind lets do it
//namespace { // anonymous

template <typename T>
T min_glue(T a, T b)
{
	return std::min(a, b);
}

template <typename T>
T max_glue(T a, T b)
{
	return std::max(a, b);
}

struct lazy_pow_
{
	template <typename X, typename Y>
	struct result { typedef X type; };
 
	template <typename X, typename Y>
	X operator()(X x, Y y) const
	{
		return std::pow(x, y);
	}
};
 
struct lazy_ufunc_
{
	template <typename F, typename A1>
	struct result { typedef A1 type; };
 
	template <typename F, typename A1>
	A1 operator()(F f, A1 a1) const
	{
		return f(a1);
	}
};
 
struct lazy_bfunc_
{
	template <typename F, typename A1, typename A2>
	struct result { typedef A1 type; };
 
	template <typename F, typename A1, typename A2>
	A1 operator()(F f, A1 a1, A2 a2) const
	{
		return f(a1, a2);
	}
};
 
//} // end namespace anonymous
 
template <typename FPT, typename Iterator>
struct grammar
	: boost::spirit::qi::grammar<
			Iterator, FPT(), boost::spirit::ascii::space_type
		>
{
 
	// symbol table for constants
	// to be added by the actual calculator
	struct constant_
		: boost::spirit::qi::symbols<
				typename std::iterator_traits<Iterator>::value_type,
				FPT
			>
	{
		constant_()
		{
		}
	} constant;
 
	// symbol table for unary functions like "abs"
	struct ufunc_
		: boost::spirit::qi::symbols<
				typename std::iterator_traits<Iterator>::value_type,
				FPT (*)(FPT)
			>
	{
		ufunc_()
		{
			this->add
				("abs"   , (FPT (*)(FPT)) std::abs  )
				("acos"  , (FPT (*)(FPT)) std::acos )
				("asin"  , (FPT (*)(FPT)) std::asin )
				("atan"  , (FPT (*)(FPT)) std::atan )
				("ceil"  , (FPT (*)(FPT)) std::ceil	)
				("cos"   , (FPT (*)(FPT)) std::cos  )
				("cosh"  , (FPT (*)(FPT)) std::cosh )
				("exp"   , (FPT (*)(FPT)) std::exp  )
				("floor" , (FPT (*)(FPT)) std::floor)
				("log"   , (FPT (*)(FPT)) std::log  )
				("log10" , (FPT (*)(FPT)) std::log10)
				("sin"   , (FPT (*)(FPT)) std::sin  )
				("sinh"  , (FPT (*)(FPT)) std::sinh )
				("sqrt"  , (FPT (*)(FPT)) std::sqrt )
				("tan"   , (FPT (*)(FPT)) std::tan  )
				("tanh"  , (FPT (*)(FPT)) std::tanh )
			;
		}
	} ufunc;

	// symbol table for binary functions like "pow"
	struct bfunc_
		: boost::spirit::qi::symbols<
				typename std::iterator_traits<Iterator>::value_type,
				FPT (*)(FPT, FPT)
			>
	{
		bfunc_()
		{
			using boost::bind;
			this->add
				("pow"  , (FPT (*)(FPT, FPT)) std::pow	)
				("atan2", (FPT (*)(FPT, FPT)) std::atan2)
				("min"	, (FPT (*)(FPT, FPT)) min_glue)
				("max"	, (FPT (*)(FPT, FPT)) max_glue)
			;
		}
	} bfunc;
 
	boost::spirit::qi::rule<
			Iterator, FPT(), boost::spirit::ascii::space_type
		> expression, term, factor, primary;
 
	grammar() : grammar::base_type(expression)
	{
		using boost::spirit::qi::real_parser;
		using boost::spirit::qi::real_policies;
		real_parser<FPT,real_policies<FPT> > real;
 
		using boost::spirit::qi::_1;
		using boost::spirit::qi::_2;
		using boost::spirit::qi::_3;
		using boost::spirit::qi::no_case;
		using boost::spirit::qi::_val;
 
		boost::phoenix::function<lazy_pow_>   lazy_pow;
		boost::phoenix::function<lazy_ufunc_> lazy_ufunc;
		boost::phoenix::function<lazy_bfunc_> lazy_bfunc;
 
		expression =
			term                   [_val =  _1]
			>> *(  ('+' >> term    [_val += _1])
				|  ('-' >> term    [_val -= _1])
				)
			;
 
		term =
			factor                 [_val =  _1]
			>> *(  ('*' >> factor  [_val *= _1])
				|  ('/' >> factor  [_val /= _1])
				)
			;
 
		factor =
			primary                [_val =  _1]
			>> *(  ("**" >> factor [_val = lazy_pow(_val, _1)])
				)
			;
 
		primary =
			real                   [_val =  _1]
			|   '(' >> expression  [_val =  _1] >> ')'
			|   ('-' >> primary    [_val = -_1])
			|   ('+' >> primary    [_val =  _1])
			|   (no_case[ufunc] >> '(' >> expression >> ')')
								   [_val = lazy_ufunc(_1, _2)]
			|   (no_case[bfunc] >> '(' >> expression >> ','
									   >> expression >> ')')
								   [_val = lazy_bfunc(_1, _2, _3)]
			|   no_case[constant]  [_val =  _1]
			;
 
	}
};
 
template <typename FPT, typename Iterator>
bool parse(Iterator &iter,
		   Iterator end,
		   const grammar<FPT,Iterator> &g,
		   FPT &result)
{
	return boost::spirit::qi::phrase_parse(
				iter, end, g, boost::spirit::ascii::space, result);
}
 
} // end namespace expression

#endif // LL_CALCPARSER_H
