/* *      Streaming XML Parser.
 *
 *      Copyright (c) 2005-2017 by Brian. Kramer
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions (known as zlib license):
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#pragma once

#include <assert.h>
#include <float.h>
#include <iomanip>
#include <limits.h>
#include <math.h>
#include <set>

#include "xmlaggr.h"

namespace StreamingXml
{

struct XmlOperator
{
    enum OpFlags
    {
        GatherData = 0x1, // no longer being used, but kept in in case we want it again
        Aggregate = 0x2,
        StartMatchEval = 0x4,
        EndMatchEval = 0x8,
        ImmedEvaluate = StartMatchEval | EndMatchEval,
        OnceOnly = 0x10,
        TopLevelOnly = 0x20,
        BinaryInfix = 0x80,
        Directive = 0x100,
        NoData = 0x200,
        UnquotedStringFirstArg = 0x400,
        UnquotedStringSecondArg = 0x800,
    };

    // Note: infix operators OpNeg => OpGT are in decreasing precedences
    // (there are no precedence classes where left- or right-associativity matters)
    enum Opcode
    {
        // clang-format off
        OpNull,
        OpColumnRef, OpPathRef, OpLiteral, // terminals
        OpNeg, OpNot, // unary -> 1-arg
        OpMul, OpDiv, OpMod, OpAdd, OpSub, OpConcat,  // 2-arg infix
        OpEQ, OpNE, OpLE, OpGE, OpLT, OpGT, //  2-arg infix
        OpOr, OpXor, OpAnd, // 2-arg infix
        OpMin, OpMax, OpSqrt, OpPow, OpLog, OpExp, OpAbs, OpRound, OpFloor, OpCeil, // 1-arg arithmetic
        OpLen, OpLeft, OpRight, OpUpper, OpLower, OpContains, OpFind, // 1-arg and 2-arg string
        OpFormatSec, OpFormatMs, OpComma, OpRowNum, OpIf, // misc
        OpReal, OpInt, OpBool, OpStr, OpDateTime, OpType, // typing
        OpPath, OpPivotPath, OpDepth, OpAttr, OpLineNum, // immediate functions (evaluated on path match)
        OpParent, OpNodeNum, OpNodeName, OpNodeStart, OpNodeEnd, // immediate functions (evaluated on path match)
        OpAny, OpSum, OpMinAggr, OpMaxAggr, OpAvg, OpStdev, OpVar, OpCov, OpCorr, OpCount, // aggregate functions
        OpFirst, OpTop, OpSort, OpPivot, OpDistinct, OpHidden, OpWhere, OpSync, OpRoot, OpIn, OpJoin, // directives
        OpCsvOnly, OpCase, OpInputHeader, OpJoinHeader, OpOutputHeader, OpSep, OpHelp // directives
        // clang-format on
    };

    XmlOperator(const std::string& name, Opcode op, size_t minArgs, size_t maxArgs, XmlType type, unsigned int flags = 0)
        : name(name)
        , opcode(op)
        , minArgs(minArgs)
        , maxArgs(maxArgs)
        , type(type)
        , flags(flags)
        , numPasses(1)
    {
        if (flags & OpFlags::Directive) {
            flags |= OpFlags::NoData;
        }
    }

    virtual ~XmlOperator() {}

    operator std::string() const
    {
        return std::string("Operator(") + name + ")";
    }

    std::string name;
    Opcode opcode;
    size_t minArgs;
    size_t maxArgs;
    XmlType type;
    unsigned int flags;
    int numPasses;
};

class XmlAggregateOperator : public XmlOperator
{
public:
    XmlAggregateOperator(XmlOperatorPtr opTemplate)
        : XmlOperator(
            opTemplate->name,
            opTemplate->opcode,
            opTemplate->minArgs,
            opTemplate->maxArgs,
            opTemplate->type,
            opTemplate->flags)
        , aggrIdx(0) // assigned by XmlColumnParser::PostprocessExprs
        , aggrType(GetAggrType(opTemplate->opcode))
    {
    }

    size_t aggrIdx;
    XmlAggrType aggrType;

    static XmlAggrType GetAggrType(Opcode opcode)
    {
        switch (opcode) {
            case XmlOperator::OpAny:
                return XmlAggrType::Any;
            case XmlOperator::OpSum:
                return XmlAggrType::Sum;
            case XmlOperator::OpAvg:
                return XmlAggrType::Avg;
            case XmlOperator::OpMinAggr:
                return XmlAggrType::Min;
            case XmlOperator::OpMaxAggr:
                return XmlAggrType::Max;
            case XmlOperator::OpVar:
                return XmlAggrType::Var;
            case XmlOperator::OpCov:
                return XmlAggrType::Cov;
            case XmlOperator::OpCorr:
                return XmlAggrType::Corr;
            case XmlOperator::OpStdev:
                return XmlAggrType::Stdev;
            case XmlOperator::OpCount:
                return XmlAggrType::Count;
            default:
                assert(false);
                return XmlAggrType::Count;
        }
    }
};

class XmlOperatorFactory
{
public:
    // Note: operator instances are pointers to support aggregate operator inheritance, where state is carried.
    // (Also, in the past, external operators implemented in Win32 DLLs were supported. That functionality was
    // removed to simplify the code base.)
    static const std::vector<XmlOperatorPtr>& GetTemplates()
    {
        const size_t U = (size_t)-1;
        static std::vector<XmlOperatorPtr> templates = {
            // clang-format off
            XmlOperatorPtr(new XmlOperator( "<ColumnRef>",XmlOperator::OpColumnRef,    0, 0, XmlType::Unknown )),
            XmlOperatorPtr(new XmlOperator( "<PathRef>",  XmlOperator::OpPathRef,      0, 0, XmlType::Unknown )),
            XmlOperatorPtr(new XmlOperator( "<Literal>",  XmlOperator::OpLiteral,      0, 0, XmlType::Unknown )),
            XmlOperatorPtr(new XmlOperator( "case",       XmlOperator::OpCase,         0, 1, XmlType::Unknown,  XmlOperator::TopLevelOnly | XmlOperator::Directive | XmlOperator::OnceOnly )),
            XmlOperatorPtr(new XmlOperator( "help",       XmlOperator::OpHelp,         0, 0, XmlType::Unknown,  XmlOperator::TopLevelOnly | XmlOperator::Directive | XmlOperator::OnceOnly )),
/*synonym*/ XmlOperatorPtr(new XmlOperator( "usage",      XmlOperator::OpHelp,         0, 0, XmlType::Unknown,  XmlOperator::TopLevelOnly | XmlOperator::Directive | XmlOperator::OnceOnly )),
            XmlOperatorPtr(new XmlOperator( "csvonly",    XmlOperator::OpCsvOnly,      0, 1, XmlType::Unknown,  XmlOperator::TopLevelOnly | XmlOperator::Directive | XmlOperator::OnceOnly )),
            XmlOperatorPtr(new XmlOperator( "in",         XmlOperator::OpIn,           1, 1, XmlType::Unknown,  XmlOperator::TopLevelOnly | XmlOperator::Directive | XmlOperator::OnceOnly | XmlOperator::UnquotedStringFirstArg )),
            XmlOperatorPtr(new XmlOperator( "inheader",   XmlOperator::OpInputHeader,  0, 1, XmlType::Unknown,  XmlOperator::TopLevelOnly | XmlOperator::Directive | XmlOperator::OnceOnly )),
            XmlOperatorPtr(new XmlOperator( "outheader",  XmlOperator::OpOutputHeader, 0, 1, XmlType::Unknown,  XmlOperator::TopLevelOnly | XmlOperator::Directive | XmlOperator::OnceOnly )),
/*synonym*/ XmlOperatorPtr(new XmlOperator( "header",     XmlOperator::OpOutputHeader, 0, 1, XmlType::Unknown,  XmlOperator::TopLevelOnly | XmlOperator::Directive | XmlOperator::OnceOnly )),
            XmlOperatorPtr(new XmlOperator( "sep",        XmlOperator::OpSep,          1, 1, XmlType::Unknown,  XmlOperator::TopLevelOnly | XmlOperator::Directive | XmlOperator::OnceOnly | XmlOperator::UnquotedStringFirstArg )),
            XmlOperatorPtr(new XmlOperator( "join",       XmlOperator::OpJoin,         1, 2, XmlType::Unknown,  XmlOperator::TopLevelOnly | XmlOperator::Directive | XmlOperator::OnceOnly | XmlOperator::UnquotedStringFirstArg )),
            XmlOperatorPtr(new XmlOperator( "joinheader", XmlOperator::OpJoinHeader,   0, 1, XmlType::Unknown,  XmlOperator::TopLevelOnly | XmlOperator::Directive | XmlOperator::OnceOnly )),
            XmlOperatorPtr(new XmlOperator( "pivot",      XmlOperator::OpPivot,        2, 3, XmlType::Unknown,  XmlOperator::TopLevelOnly | XmlOperator::Directive | XmlOperator::OnceOnly )),
            XmlOperatorPtr(new XmlOperator( "..",         XmlOperator::OpAttr,         2, 2, XmlType::String, XmlOperator::NoData | XmlOperator::StartMatchEval | XmlOperator::BinaryInfix )),
            XmlOperatorPtr(new XmlOperator( "rownum",     XmlOperator::OpRowNum,       0, 0, XmlType::Integer )),
            XmlOperatorPtr(new XmlOperator( "linenum",    XmlOperator::OpLineNum,      1, 1, XmlType::Integer,  XmlOperator::NoData | XmlOperator::StartMatchEval )),
            XmlOperatorPtr(new XmlOperator( "depth",      XmlOperator::OpDepth,        1, 1, XmlType::Integer,  XmlOperator::NoData | XmlOperator::StartMatchEval )),
            XmlOperatorPtr(new XmlOperator( "sync",       XmlOperator::OpSync,         1, 1, XmlType::Unknown,  XmlOperator::TopLevelOnly | XmlOperator::Directive | XmlOperator::OnceOnly | XmlOperator::EndMatchEval )),
            XmlOperatorPtr(new XmlOperator( "root",       XmlOperator::OpRoot,         1, 1, XmlType::Unknown,  XmlOperator::TopLevelOnly | XmlOperator::Directive | XmlOperator::OnceOnly | XmlOperator::UnquotedStringFirstArg )),
            XmlOperatorPtr(new XmlOperator( "path",       XmlOperator::OpPath,         1, 1, XmlType::String,   XmlOperator::NoData | XmlOperator::StartMatchEval )),
            XmlOperatorPtr(new XmlOperator( "pivotpath",  XmlOperator::OpPivotPath,    1, 1, XmlType::String,   XmlOperator::NoData | XmlOperator::StartMatchEval | XmlOperator::TopLevelOnly | XmlOperator::OnceOnly )),
            XmlOperatorPtr(new XmlOperator( "parent",     XmlOperator::OpParent,       1, 1, XmlType::String,   XmlOperator::NoData | XmlOperator::StartMatchEval )),
            XmlOperatorPtr(new XmlOperator( "nodename",   XmlOperator::OpNodeName,     1, 2, XmlType::String,   XmlOperator::NoData | XmlOperator::StartMatchEval )),
            XmlOperatorPtr(new XmlOperator( "nodenum",    XmlOperator::OpNodeNum,      1, 2, XmlType::Integer,  XmlOperator::NoData | XmlOperator::StartMatchEval | XmlOperator::UnquotedStringSecondArg )),
            XmlOperatorPtr(new XmlOperator( "nodestart",  XmlOperator::OpNodeStart,    1, 1, XmlType::Integer,  XmlOperator::NoData | XmlOperator::StartMatchEval | XmlOperator::UnquotedStringSecondArg )),
            XmlOperatorPtr(new XmlOperator( "nodeend",    XmlOperator::OpNodeEnd,      1, 1, XmlType::Integer,  XmlOperator::NoData | XmlOperator::EndMatchEval | XmlOperator::UnquotedStringSecondArg )),
            XmlOperatorPtr(new XmlOperator( "where",      XmlOperator::OpWhere,        1, 1, XmlType::Unknown,  XmlOperator::TopLevelOnly | XmlOperator::Directive )),
            XmlOperatorPtr(new XmlOperator( "first",      XmlOperator::OpFirst,        1, 1, XmlType::Unknown,  XmlOperator::TopLevelOnly | XmlOperator::Directive | XmlOperator::OnceOnly )),
            XmlOperatorPtr(new XmlOperator( "top",        XmlOperator::OpTop,          1, 1, XmlType::Unknown,  XmlOperator::TopLevelOnly | XmlOperator::Directive | XmlOperator::OnceOnly )),
            XmlOperatorPtr(new XmlOperator( "sort",       XmlOperator::OpSort,         1, U, XmlType::Unknown,  XmlOperator::TopLevelOnly | XmlOperator::Directive | XmlOperator::OnceOnly )),
            XmlOperatorPtr(new XmlOperator( "distinct",   XmlOperator::OpDistinct,     0, 1, XmlType::Unknown,  XmlOperator::TopLevelOnly | XmlOperator::Directive | XmlOperator::OnceOnly )),
            XmlOperatorPtr(new XmlOperator( "hidden",     XmlOperator::OpHidden,       1, 1, XmlType::Unknown,  XmlOperator::TopLevelOnly )),
            XmlOperatorPtr(new XmlOperator( "not",        XmlOperator::OpNot,          1, 1, XmlType::Boolean )),
            XmlOperatorPtr(new XmlOperator( "!",          XmlOperator::OpNot,          1, 1, XmlType::Boolean )),
            XmlOperatorPtr(new XmlOperator( "-" ,         XmlOperator::OpNeg,          1, 1, XmlType::Real )),
            XmlOperatorPtr(new XmlOperator( "*",          XmlOperator::OpMul,          2, 2, XmlType::Real, XmlOperator::BinaryInfix )),
            XmlOperatorPtr(new XmlOperator( "/",          XmlOperator::OpDiv,          2, 2, XmlType::Real, XmlOperator::BinaryInfix )),
            XmlOperatorPtr(new XmlOperator( "%",          XmlOperator::OpMod,          2, 2, XmlType::Integer, XmlOperator::BinaryInfix )),
            XmlOperatorPtr(new XmlOperator( "+",          XmlOperator::OpAdd,          1, 2, XmlType::Real, XmlOperator::BinaryInfix )),
            XmlOperatorPtr(new XmlOperator( "-",          XmlOperator::OpSub,          2, 2, XmlType::Real, XmlOperator::BinaryInfix )),
            XmlOperatorPtr(new XmlOperator( "eq",         XmlOperator::OpEQ,           2, 2, XmlType::Boolean )),
            XmlOperatorPtr(new XmlOperator( "==",         XmlOperator::OpEQ,           2, 2, XmlType::Boolean, XmlOperator::BinaryInfix )),
            XmlOperatorPtr(new XmlOperator( "ne",         XmlOperator::OpNE,           2, 2, XmlType::Boolean )),
            XmlOperatorPtr(new XmlOperator( "!=",         XmlOperator::OpNE,           2, 2, XmlType::Boolean, XmlOperator::BinaryInfix )),
            XmlOperatorPtr(new XmlOperator( "le",         XmlOperator::OpLE,           2, 2, XmlType::Boolean )),
            XmlOperatorPtr(new XmlOperator( "<=",         XmlOperator::OpLE,           2, 2, XmlType::Boolean, XmlOperator::BinaryInfix )),
            XmlOperatorPtr(new XmlOperator( "ge",         XmlOperator::OpGE,           2, 2, XmlType::Boolean )),
            XmlOperatorPtr(new XmlOperator( ">=",         XmlOperator::OpGE,           2, 2, XmlType::Boolean, XmlOperator::BinaryInfix )),
            XmlOperatorPtr(new XmlOperator( "lt",         XmlOperator::OpLT,           2, 2, XmlType::Boolean )),
            XmlOperatorPtr(new XmlOperator( "<",          XmlOperator::OpLT,           2, 2, XmlType::Boolean, XmlOperator::BinaryInfix )),
            XmlOperatorPtr(new XmlOperator( "gt",         XmlOperator::OpGT,           2, 2, XmlType::Boolean )),
            XmlOperatorPtr(new XmlOperator( ">",          XmlOperator::OpGT,           2, 2, XmlType::Boolean, XmlOperator::BinaryInfix )),
            XmlOperatorPtr(new XmlOperator( "and",        XmlOperator::OpAnd,          2, 2, XmlType::Boolean )),
            XmlOperatorPtr(new XmlOperator( "&&",         XmlOperator::OpAnd,          2, 2, XmlType::Boolean, XmlOperator::BinaryInfix )),
            XmlOperatorPtr(new XmlOperator( "or",         XmlOperator::OpOr,           2, 2, XmlType::Boolean )),
            XmlOperatorPtr(new XmlOperator( "||",         XmlOperator::OpOr,           2, 2, XmlType::Boolean, XmlOperator::BinaryInfix )),
            XmlOperatorPtr(new XmlOperator( "xor",        XmlOperator::OpXor,          2, 2, XmlType::Boolean )),
            XmlOperatorPtr(new XmlOperator( "^",          XmlOperator::OpXor,          2, 2, XmlType::Boolean, XmlOperator::BinaryInfix )),
            XmlOperatorPtr(new XmlOperator( "if",         XmlOperator::OpIf,           3, 3, XmlType::Real )),  // retyped as needed
            XmlOperatorPtr(new XmlOperator( "abs",        XmlOperator::OpAbs,          1, 1, XmlType::Real )),
            XmlOperatorPtr(new XmlOperator( "floor",      XmlOperator::OpFloor,        1, 1, XmlType::Real )),
            XmlOperatorPtr(new XmlOperator( "ceil",       XmlOperator::OpCeil,         1, 1, XmlType::Real )),
            XmlOperatorPtr(new XmlOperator( "round",      XmlOperator::OpRound,        1, 2, XmlType::Real )),
            XmlOperatorPtr(new XmlOperator( "min",        XmlOperator::OpMin,          2, 2, XmlType::Real )),  // may turn into OpMinAggr
            XmlOperatorPtr(new XmlOperator( "max",        XmlOperator::OpMax,          2, 2, XmlType::Real )),  // may turn into OpMaxAggr
            XmlOperatorPtr(new XmlOperator( "sqrt",       XmlOperator::OpSqrt,         1, 1, XmlType::Real )),
            XmlOperatorPtr(new XmlOperator( "pow",        XmlOperator::OpPow,          2, 2, XmlType::Real )),
            XmlOperatorPtr(new XmlOperator( "log",        XmlOperator::OpLog,          1, 2, XmlType::Real )),  // default base e
            XmlOperatorPtr(new XmlOperator( "exp",        XmlOperator::OpExp,          1, 1, XmlType::Real )),
            XmlOperatorPtr(new XmlOperator( "&",          XmlOperator::OpConcat,       2, 2, XmlType::String, XmlOperator::BinaryInfix )),
/*synonym*/ XmlOperatorPtr(new XmlOperator( "concat",     XmlOperator::OpConcat,       2, 2, XmlType::String )),
            XmlOperatorPtr(new XmlOperator( "len",        XmlOperator::OpLen,          1, 1, XmlType::Integer )),
            XmlOperatorPtr(new XmlOperator( "left",       XmlOperator::OpLeft,         2, 2, XmlType::String )),
            XmlOperatorPtr(new XmlOperator( "right",      XmlOperator::OpRight,        2, 2, XmlType::String )),
            XmlOperatorPtr(new XmlOperator( "lower",      XmlOperator::OpLower,        1, 1, XmlType::String )),
            XmlOperatorPtr(new XmlOperator( "upper",      XmlOperator::OpUpper,        1, 1, XmlType::String )),
            XmlOperatorPtr(new XmlOperator( "contains",   XmlOperator::OpContains,     2, 2, XmlType::Boolean )),
            XmlOperatorPtr(new XmlOperator( "find",       XmlOperator::OpFind,         2, 2, XmlType::Integer )),
            XmlOperatorPtr(new XmlOperator( "formatsec",  XmlOperator::OpFormatSec,    1, 1, XmlType::String)),
            XmlOperatorPtr(new XmlOperator( "formatms",   XmlOperator::OpFormatMs,     1, 1, XmlType::String)),
            XmlOperatorPtr(new XmlOperator( "comma",      XmlOperator::OpComma,        1, 1, XmlType::String)),
            XmlOperatorPtr(new XmlOperator( "type",       XmlOperator::OpType,         1, 1, XmlType::String )),
            XmlOperatorPtr(new XmlOperator( "real",       XmlOperator::OpReal,         1, 1, XmlType::Real)),
            XmlOperatorPtr(new XmlOperator( "int",        XmlOperator::OpInt,          1, 1, XmlType::Integer)),
            XmlOperatorPtr(new XmlOperator( "bool",       XmlOperator::OpBool,         1, 1, XmlType::Boolean)),
            XmlOperatorPtr(new XmlOperator( "str",        XmlOperator::OpStr,          1, 2, XmlType::String)),
            XmlOperatorPtr(new XmlOperator( "datetime",   XmlOperator::OpDateTime,     1, 1, XmlType::DateTime)),
            XmlOperatorPtr(new XmlOperator( "any",        XmlOperator::OpAny,          1, 1, XmlType::String,   XmlOperator::Aggregate )),
            XmlOperatorPtr(new XmlOperator( "sum",        XmlOperator::OpSum,          1, 1, XmlType::Real,     XmlOperator::Aggregate )),
            XmlOperatorPtr(new XmlOperator( "avg",        XmlOperator::OpAvg,          1, 1, XmlType::Real,     XmlOperator::Aggregate )),
            XmlOperatorPtr(new XmlOperator( "min",        XmlOperator::OpMinAggr,      1, 1, XmlType::Real,     XmlOperator::Aggregate )),  // special case: meaning depends on # args
            XmlOperatorPtr(new XmlOperator( "max",        XmlOperator::OpMaxAggr,      1, 1, XmlType::Real,     XmlOperator::Aggregate )),
            XmlOperatorPtr(new XmlOperator( "var",        XmlOperator::OpVar,          1, 1, XmlType::Real,     XmlOperator::Aggregate )),
            XmlOperatorPtr(new XmlOperator( "cov",        XmlOperator::OpCov,          2, 2, XmlType::Real,     XmlOperator::Aggregate )),
            XmlOperatorPtr(new XmlOperator( "corr",       XmlOperator::OpCorr,         2, 2, XmlType::Real,     XmlOperator::Aggregate )),
            XmlOperatorPtr(new XmlOperator( "stdev",      XmlOperator::OpStdev,        1, 1, XmlType::Real,     XmlOperator::Aggregate )),
            XmlOperatorPtr(new XmlOperator( "count",      XmlOperator::OpCount,        1, 1, XmlType::Integer,  XmlOperator::NoData | XmlOperator::Aggregate )),
            // clang-format on
        };
        return templates;
    }

    static std::string GetCategory(XmlOperator::Opcode opcode)
    {
        using Op = XmlOperator;
        switch (opcode) {
            case Op::OpFirst: case Op::OpTop: case Op::OpSort: case Op::OpPivot: case Op::OpDistinct: case Op::OpHidden:
            case Op::OpWhere: case Op::OpSync: case Op::OpRoot: case Op::OpIn: case Op::OpJoin:
            case Op::OpCsvOnly: case Op::OpCase: case Op::OpInputHeader: case Op::OpJoinHeader:
            case Op::OpOutputHeader: case Op::OpSep: case Op::OpHelp:
                return "Directives";

            case Op::OpAny: case Op::OpSum: case Op::OpMinAggr: case Op::OpMaxAggr: case Op::OpAvg: case Op::OpStdev:
            case Op::OpVar: case Op::OpCov: case Op::OpCorr: case Op::OpCount:
                return "Aggregate operators";

            case Op::OpLen: case Op::OpLeft: case Op::OpRight: case Op::OpUpper: case Op::OpLower:
            case Op::OpContains: case Op::OpFind:
                return "String operators";

            case Op::OpMin: case Op::OpMax: case Op::OpSqrt: case Op::OpPow: case Op::OpLog: case Op::OpExp:
            case Op::OpAbs: case Op::OpRound: case Op::OpFloor: case Op::OpCeil:
                return "Math operators";

            case Op::OpReal: case Op::OpInt: case Op::OpBool: case Op::OpStr: case Op::OpDateTime: case Op::OpType:
                return "Type conversion operators";

            case Op::OpPath: case Op::OpPivotPath: case Op::OpDepth: case Op::OpAttr: case Op::OpLineNum:
            case Op::OpParent: case Op::OpNodeNum: case Op::OpNodeName: case Op::OpNodeStart: case Op::OpNodeEnd:
                return "Structural operators";

            case Op::OpFormatSec: case Op::OpFormatMs: case Op::OpComma: case Op::OpRowNum: case Op::OpIf:
                return "Misc operators";

            case Op::OpNeg: case Op::OpNot:
            case Op::OpMul: case Op::OpDiv: case Op::OpMod: case Op::OpAdd: case Op::OpSub: case Op::OpConcat:
            case Op::OpEQ: case Op::OpNE: case Op::OpLE: case Op::OpGE: case Op::OpLT: case Op::OpGT:
            case Op::OpOr: case Op::OpXor: case Op::OpAnd:
                return "Infix operators";

            default:
                return "";
        }
    }

    static std::string GetDescription(XmlOperator::Opcode opcode)
    {
        using Op = XmlOperator;
        switch (opcode) {
            case Op::OpFirst: return "Limits to the first n input rows, before filtering and sorting";
            case Op::OpTop: return "Limits to the top n output rows, after filtering and sorting";
            case Op::OpSort: return "Sorts output rows by one or more expressions; prefix an expression with - for descending";
            case Op::OpPivot: return "Spreads repeated values into separate output columns";
            case Op::OpDistinct: return "Removes duplicate output rows";
            case Op::OpHidden: return "Evaluates an expression as a named intermediate value, without adding an output column for it";
            case Op::OpWhere: return "Filters output rows to those where the given predicate is true";
            case Op::OpSync: return "Emits a new output row each time the given path repeats, for combining sibling elements that don't share a common repeating parent";
            case Op::OpRoot: return "Selects the nth top-level node as the document root for path matching";
            case Op::OpIn: return "Reads input from a file instead of standard input";
            case Op::OpJoin: return "Joins another file's rows, matched with a where[] equality condition; pass true as a second argument for an outer join";
            case Op::OpCsvOnly: return "Skips JSON/XML/log-format detection and parses the input strictly as CSV/TSV";
            case Op::OpCase: return "Enables case-sensitive matching (off by default)";
            case Op::OpInputHeader: return "Whether the input CSV/TSV has a header row (default: true)";
            case Op::OpJoinHeader: return "Whether the joined file has a header row (default: true)";
            case Op::OpOutputHeader: return "Whether to print a header row in the output (default: true)";
            case Op::OpSep: return "Sets the output field separator (default: tab); also accepts the named alias \"tab\"";
            case Op::OpHelp: return "Prints this help text";

            case Op::OpAny: return "First non-empty value encountered in the group";
            case Op::OpSum: return "Sum of a numeric expression across the group";
            case Op::OpMinAggr: return "Smallest value of an expression across the group";
            case Op::OpMaxAggr: return "Largest value of an expression across the group";
            case Op::OpAvg: return "Average (mean) of a numeric expression across the group";
            case Op::OpStdev: return "Sample standard deviation of a numeric expression across the group";
            case Op::OpVar: return "Sample variance of a numeric expression across the group";
            case Op::OpCov: return "Covariance of two numeric expressions across the group";
            case Op::OpCorr: return "Correlation coefficient of two numeric expressions across the group";
            case Op::OpCount: return "Number of rows in the group";

            case Op::OpLen: return "Length of a string";
            case Op::OpLeft: return "First n characters of a string";
            case Op::OpRight: return "Last n characters of a string";
            case Op::OpUpper: return "Converts a string to uppercase";
            case Op::OpLower: return "Converts a string to lowercase";
            case Op::OpContains: return "True if the first string contains the second";
            case Op::OpFind: return "Index of the second string within the first, or -1 if not found";

            case Op::OpMin: return "Smaller of two values";
            case Op::OpMax: return "Larger of two values";
            case Op::OpSqrt: return "Square root";
            case Op::OpPow: return "First value raised to the power of the second";
            case Op::OpLog: return "Logarithm, base e unless a second argument gives the base";
            case Op::OpExp: return "e raised to the power of the value";
            case Op::OpAbs: return "Absolute value";
            case Op::OpRound: return "Rounds to the given number of decimal places (0 by default)";
            case Op::OpFloor: return "Rounds down to the nearest integer";
            case Op::OpCeil: return "Rounds up to the nearest integer";

            case Op::OpReal: return "Converts to a real (floating-point) number";
            case Op::OpInt: return "Converts to an integer";
            case Op::OpBool: return "Converts to a boolean";
            case Op::OpStr: return "Converts to a string";
            case Op::OpDateTime: return "Converts to a datetime value";
            case Op::OpType: return "Name of a value's type (\"int\", \"real\", \"str\", \"bool\", or \"datetime\")";

            case Op::OpPath: return "Fully-qualified path of the matched node";
            case Op::OpPivotPath: return "Pivoted path (dot-separated ancestor names) of the matched node, for use inside pivot[]";
            case Op::OpDepth: return "Nesting depth of the matched node";
            case Op::OpAttr: return "Infix form (element..attribute) that accesses an XML attribute's value";
            case Op::OpLineNum: return "Line number in the input where the matched node appears";
            case Op::OpParent: return "Name of the immediate parent node (shorthand for nodename[path,1])";
            case Op::OpNodeNum: return "Position of the matched node in document order, optionally relative to a named or leveled ancestor";
            case Op::OpNodeName: return "Name of the matched node, or an ancestor N levels up if a second argument is given";
            case Op::OpNodeStart: return "Byte offset in the input where the matched node begins";
            case Op::OpNodeEnd: return "Byte offset in the input where the matched node ends";

            case Op::OpFormatSec: return "Formats a Unix timestamp in seconds as a readable date/time";
            case Op::OpFormatMs: return "Formats a Unix timestamp in milliseconds as a readable date/time";
            case Op::OpComma: return "Formats a number with thousands separators, e.g. comma[1234567] -> \"1,234,567\"";
            case Op::OpRowNum: return "The 1-based row number of the current output row";
            case Op::OpIf: return "Returns the second argument if the first is true, otherwise the third";

            default: return "";
        }
    }

    static std::string GetHelpText()
    {
        static const std::vector<std::string> categoryOrder = {
            "Directives", "Aggregate operators", "String operators", "Math operators",
            "Type conversion operators", "Structural operators", "Misc operators",
            "Infix operators"
        };

        // {category, signature, description}
        std::vector<std::tuple<std::string, std::string, std::string>> allEntries;
        for (auto& category : categoryOrder) {
            bool showDescriptions = (category != "Infix operators");
            for (auto& op : GetTemplates()) {
                if (GetCategory(op->opcode) == category) {
                    std::stringstream sig;
                    sig << op->name;
                    if (op->maxArgs > 0) {
                        sig << "[";
                        if (op->minArgs != op->maxArgs) {
                            sig << op->minArgs << "-";
                        }
                        sig << (op->maxArgs == (size_t)-1 ? std::string("N") : std::to_string(op->maxArgs));
                        sig << (op->maxArgs == 1 && op->minArgs == 1 ? " arg]" : " args]");
                    }
                    allEntries.push_back({category, sig.str(), showDescriptions ? GetDescription(op->opcode) : ""});
                }
            }
        }

        // Width excludes infix operators, which never show a description.
        size_t widest = 0;
        for (auto& entry : allEntries) {
            if (!std::get<2>(entry).empty()) {
                widest = std::max(widest, std::get<1>(entry).size());
            }
        }

        std::stringstream out;
        out << "Every operator below that takes at least one argument can be written either as a "
               "flag, e.g. --sep=tab, or as a function, e.g. sep[tab]. The choice is a matter of "
               "style. Infix operators (last, below) are ordinarily written inline using their "
               "symbol instead, e.g. a+b rather than add[a,b].\n\n";
        std::string currentCategory;
        for (auto& entry : allEntries) {
            const std::string& category = std::get<0>(entry);
            const std::string& sig = std::get<1>(entry);
            const std::string& desc = std::get<2>(entry);
            if (category != currentCategory) {
                out << category << ":\n";
                currentCategory = category;
            }
            out << "  " << sig;
            if (!desc.empty()) {
                out << std::string(widest - sig.size(), ' ') << "  " << desc;
            }
            out << "\n";
        }
        return out.str();
    }

    static XmlOperatorPtr GetInstance(XmlOperator::Opcode opcode, const std::string& name = std::string())
    {
        assert(name.empty() || (name[0] != '('));
        const std::vector<XmlOperatorPtr>& templates = GetTemplates();

        XmlOperatorPtr opTemplate;
        for (size_t i = 0; i < templates.size(); i++) {
            size_t len = templates[i]->name.size();
            if ((opcode == templates[i]->opcode) ||
                XmlUtils::stringsEqCase(name.c_str(), templates[i]->name)) {
                opTemplate = templates[i];
                break;
            }
        }
        if (!opTemplate) {
            XmlUtils::Error("Unrecognized function: %s", name);
        }

        XmlOperatorPtr op = opTemplate;
        if (op->flags & XmlOperator::Aggregate) {
            // Turn this operator into an aggregate operator which carries more state
            op.reset(new XmlAggregateOperator(op));
        }

        return op;
    }

    static XmlOperatorPtr GetInstance(const std::string& name)
    {
        return GetInstance(XmlOperator::OpNull, name);
    }
};

typedef XmlOperator::Opcode Opcode;

} // namespace StreamingXml

// Add more template specializations for _print (see end of xmlutils.h)
template <> void _print(StreamingXml::XmlExprPtr expr)
{
    std::cout << std::string("Expr:") + (expr->GetOperator().get()
        ? std::string(*expr->GetOperator().get())
        : "no-operator"
    );
}
