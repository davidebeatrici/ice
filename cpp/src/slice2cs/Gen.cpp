// **********************************************************************
//
// Copyright (c) 2003
// ZeroC, Inc.
// Billerica, MA, USA
//
// All Rights Reserved.
//
// Ice is free software; you can redistribute it and/or modify it under
// the terms of the GNU General Public License version 2 as published by
// the Free Software Foundation.
//
// **********************************************************************

#include <IceUtil/Functional.h>
#include <Gen.h>
#include <limits>
#include <IceUtil/Algorithm.h>
#include <IceUtil/Iterator.h>

using namespace std;
using namespace Slice;

//
// Don't use "using namespace IceUtil", or stupid VC++ 6.0 complains
// about ambigious symbols for constructs like
// "IceUtil::constMemFun(&Slice::Exception::isLocal)".
//
using IceUtil::Output;
using IceUtil::nl;
using IceUtil::sp;
using IceUtil::sb;
using IceUtil::eb;

static string // Should be an anonymous namespace, but VC++ 6 can't handle that.
sliceModeToIceMode(const OperationPtr& op)
{
    string mode;
    switch(op->mode())
    {
	case Operation::Normal:
	{
	    mode = "Ice.OperationMode.Normal";
	    break;
	}
	case Operation::Nonmutating:
	{
	    mode = "Ice.OperationMode.Nonmutating";
	    break;
	}
	case Operation::Idempotent:
	{
	    mode = "Ice.OperationMode.Idempotent";
	    break;
	}
	default:
	{
	    assert(false);
	    break;
	}
    }
    return mode;
}

Slice::CsVisitor::CsVisitor(Output& out) : _out(out)
{
}

Slice::CsVisitor::~CsVisitor()
{
}

Slice::Gen::Gen(const string& name, const string& base, const vector<string>& includePaths) :
    _base(base),
    _includePaths(includePaths)
{
    string file = base + ".cs";
    _out.open(file.c_str());
    if(!_out)
    {
        cerr << name << ": can't open `" << file << "' for writing: " << strerror(errno) << endl;
	return;
    }
    printHeader();
    _out << nl << "// Generated from file `" << name << "'";
}

Slice::Gen::~Gen()
{
}

bool
Slice::Gen::operator!() const
{
    return !_out;
}

void
Slice::Gen::generate(const UnitPtr& p)
{
    OpsVisitor opsVisitor(_out);
    p->visit(&opsVisitor);
    TypesVisitor typesVisitor(_out);
    p->visit(&typesVisitor);
    ProxyVisitor proxyVisitor(_out);
    p->visit(&proxyVisitor);
}

void
Slice::Gen::printHeader()
{
    static const char* header =
"// **********************************************************************\n"
"//\n"
"// Copyright (c) 2003\n"
"// ZeroC, Inc.\n"
"// Billerica, MA, USA\n"
"//\n"
"// All Rights Reserved.\n"
"//\n"
"// Ice is free software; you can redistribute it and/or modify it under\n"
"// the terms of the GNU General Public License version 2 as published by\n"
"// the Free Software Foundation.\n"
"//\n"
"// **********************************************************************\n"
        ;

    _out << header;
    _out << "\n// Ice version " << ICE_STRING_VERSION;
}

Slice::Gen::OpsVisitor::OpsVisitor(IceUtil::Output& out) :
    CsVisitor(out)
{
}

bool
Slice::Gen::OpsVisitor::visitModuleStart(const ModulePtr& p)
{
    string name = fixKwd(fixGlobal(p));
    _out << sp << nl << "namespace " << name;
    _out << sb;
    _out << nl << "#region " << name << " members";

    return true;
}

void
Slice::Gen::OpsVisitor::visitModuleEnd(const ModulePtr& p)
{
    _out << sp << nl << "#endregion"; // module
    _out << eb;
}

bool
Slice::Gen::OpsVisitor::visitClassDefStart(const ClassDefPtr& p)
{
    //
    // Don't generate an _Operations interface for non-abstract classes.
    //
    if(!p->isAbstract())
    {
	return false;
    }

    string name = fixKwd(fixGlobal(p));
    string scoped = fixKwd(p->scoped());
    ClassList bases = p->bases();

    _out << sp << nl << "public interface " << name << "_Operations";
    if((bases.size() == 1 && bases.front()->isAbstract()) || bases.size() > 1)
    {
        _out << " : ";
	_out.useCurrentPosAsIndent();
	ClassList::const_iterator q = bases.begin();
	bool first = true;
	while(q != bases.end())
	{
	    if((*q)->isAbstract())
	    {
	        if(!first)
		{
		    _out << ',' << nl;
		}
		else
		{
		    first = false;
		}
		_out << fixKwd((*q)->scoped()) << "_Operations";
	    }
	    ++q;
	}
	_out.restoreIndent();
    }

    _out << sb;
    _out << sp << nl << "#region " << name << "_Operations members";

    return true;
}

void
Slice::Gen::OpsVisitor::visitClassDefEnd(const ClassDefPtr& p)
{
    _out << sp << nl << "#endregion"; // _Operations members
    _out << eb;
}

void
Slice::Gen::OpsVisitor::visitOperation(const OperationPtr& p)
{
    string name = fixKwd(p->name());

    _out << sp << nl << typeToString(p->returnType()) << " " << name << "(";
    ParamDeclList paramList = p->parameters();
    for(ParamDeclList::const_iterator q = paramList.begin(); q != paramList.end(); ++q)
    {
	if(q != paramList.begin())
	{
	    _out << ", ";
	}
	if((*q)->isOutParam())
	{
	    _out << "ref ";
	}
	_out << typeToString((*q)->type()) << " " << fixKwd((*q)->name());
    }
    if(!ClassDefPtr::dynamicCast(p->container())->isLocal())
    {
        if(!paramList.empty())
	{
	    _out << ", ";
	}
	_out << "Ice.Current __current";
    }
    _out << ");";
}

Slice::Gen::TypesVisitor::TypesVisitor(IceUtil::Output& out) :
    CsVisitor(out)
{
}

bool
Slice::Gen::TypesVisitor::visitModuleStart(const ModulePtr& p)
{
    string name = fixKwd(fixGlobal(p));
    _out << sp << nl << "namespace " << name;
    _out << sb;
    _out << nl << "#region " << name << " members";

    return true;
}

void
Slice::Gen::TypesVisitor::visitModuleEnd(const ModulePtr& p)
{
    _out << sp << nl << "#endregion"; // module
    _out << eb;
}

bool
Slice::Gen::TypesVisitor::visitClassDefStart(const ClassDefPtr& p)
{
    string name = fixKwd(fixGlobal(p));
    string scoped = fixKwd(p->scoped());
    ClassList bases = p->bases();

    if(p->isInterface())
    {
        _out << sp << nl << "public interface " << name << " : ";
	_out.useCurrentPosAsIndent();
	if(p->isLocal())
	{
	    _out << "Ice.LocalObject";
	}
	else
	{
	    _out << "Ice.Object";
	}
	_out << ',' << nl << name << "_Operations";
	if(bases.empty())
	{
	    ClassList::const_iterator q = bases.begin();
	    while(q != bases.end())
	    {
	        _out << ',' << nl << fixKwd((*q)->scoped());
	    }
	}
	_out.restoreIndent();
    }
    else
    {
	_out << sp << nl << "public ";
	if(p->isAbstract())
	{
	    _out << "abstract ";
	}
	_out << "class " << name << " : ";

	_out.useCurrentPosAsIndent();
	if(bases.empty() || bases.front()->isInterface())
	{
	    if(p->isLocal())
	    {
		_out << "Ice.LocalObjectImpl";
	    }
	    else
	    {
		_out << "Ice.ObjectImpl";
	    }
	}
	else
	{
	    _out << fixKwd(bases.front()->scoped());
	    bases.pop_front();
	}
	_out << ',' << nl << name << "_Operations";
	for(ClassList::const_iterator q = bases.begin(); q != bases.end(); ++q)
	{
	    _out << ',' << nl << fixKwd((*q)->scoped()) << "_Operations";
	}
	_out.restoreIndent();
    }

    _out << sb;

    if(!p->isInterface())
    {
	_out << sp << nl << "#region " << name << " members";
	_out << sp << nl << "#region Slice data members and operations";
    }

    return true;
}

void
Slice::Gen::TypesVisitor::visitClassDefEnd(const ClassDefPtr& p)
{
    string name = fixKwd(fixGlobal(p));
    string scoped = fixKwd(p->scoped());
    DataMemberList dataMembers = p->dataMembers();
    DataMemberList::const_iterator q;


    if(!p->isInterface())
    {
	_out << sp << nl << "#endregion"; // Slice data members and operations

	ClassList bases = p->bases();
	if(!bases.empty() && !bases.front()->isInterface())
	{
	    bases.pop_front();
	}
	if(!bases.empty())
	{
	    _out << sp << nl << "#region Inherited Slice operations";

            OperationList allOps;
	    for(ClassList::const_iterator q = bases.begin(); q != bases.end(); ++q)
	    {
                OperationList tmp = (*q)->allOperations();
		allOps.splice(allOps.end(), tmp);
	    }
	    allOps.sort();
	    allOps.unique();
	    for(OperationList::const_iterator op = allOps.begin(); op != allOps.end(); ++op)
	    {
		string name = fixKwd((*op)->name());
		string scoped = fixKwd((*op)->scoped());

		_out << sp << nl << "public abstract " << typeToString((*op)->returnType()) << " " << name << "(";
		ParamDeclList paramList = (*op)->parameters();
		for(ParamDeclList::const_iterator q = paramList.begin(); q != paramList.end(); ++q)
		{
		    if(q != paramList.begin())
		    {
			_out << ", ";
		    }
		    if((*q)->isOutParam())
		    {
			_out << "ref ";
		    }
		    _out << typeToString((*q)->type()) << " " << fixKwd((*q)->name());
		}
		if(!p->isLocal())
		{
		    if(!paramList.empty())
		    {
			_out << ", ";
		    }
		    _out << "Ice.Current __current";
		}
		_out << ");";
	    }

	    _out << sp << nl << "#endregion"; // Inherited Slice operations
	}

	_out << sp << nl << "#region IComparable members";

	_out << sp << nl << "public override int CompareTo(object __other)";
	_out << sb;
	_out << nl << "if(__other == null)";
	_out << sb;
	_out << nl << "return 1;";
	_out << eb;
	_out << nl << "if(object.ReferenceEquals(this, __other))";
	_out << sb;
	_out << nl << "return 0;";
	_out << eb;
	_out << nl << "if(!(__other is " << name << "))";
	_out << sb;
	_out << nl << "throw new System.ArgumentException(\"expected argument of type `"
	     << name << "'\", \"__other\");";
	_out << eb;
	_out << nl << "int __ret = base.CompareTo(__other);";
	_out << nl << "if(__ret != 0)";
	_out << sb;
	_out << nl << "return __ret;";
	_out << eb;
	for(q = dataMembers.begin(); q != dataMembers.end(); ++q)
	{
	    string memberName = fixKwd((*q)->name());
	    if(!isValueType((*q)->type()))
	    {
		_out << nl << "if(" << memberName << " == null && ((" << name << ")__other)."
		     << memberName << " != null)";
		_out << sb;
		_out << nl << "return -1;";
		_out << eb;
	    }
	    _out << nl << "if((__ret = " << memberName << ".CompareTo(((" << name << ")__other)."
		 << memberName << ")) != 0)";
	    _out << sb;
	    _out << nl << "return __ret;";
	    _out << eb;
	}
	_out << nl << "return 0;";

	_out << eb;

	_out << sp << nl << "#endregion"; // IComparable members

	_out << sp << nl << "#region Comparison operators";

	_out << sp << nl << "public static bool operator==(" << name << " __lhs, " << name << " __rhs)";
	_out << sb;
	_out << nl << "return Equals(__lhs, __rhs);";
	_out << eb;

	_out << sp << nl << "public static bool operator!=(" << name << " __lhs, " << name << " __rhs)";
	_out << sb;
	_out << nl << "return !Equals(__lhs, __rhs);";
	_out << eb;

	_out << sp << nl << "public static bool operator<(" << name << " __lhs, " << name << " __rhs)";
	_out << sb;
	_out << nl << "return __lhs == null ? __rhs != null : __lhs.CompareTo(__rhs) < 0;";
	_out << eb;

	_out << sp << nl << "public static bool operator>(" << name << " __lhs, " << name << " __rhs)";
	_out << sb;
	_out << nl << "return __lhs == null ? false : __lhs.CompareTo(__rhs) > 0;";
	_out << eb;

	_out << sp << nl << "public static bool operator<=(" << name << " __lhs, " << name << " __rhs)";
	_out << sb;
	_out << nl << "return __lhs == null ? true : __lhs.CompareTo(__rhs) <= 0;";
	_out << eb;

	_out << sp << nl << "public static bool operator>=(" << name << " __lhs, " << name << " __rhs)";
	_out << sb;
	_out << nl << "return __lhs == null ? __rhs == null : __lhs.CompareTo(__rhs) >= 0;";
	_out << eb;

	_out << sp << nl << "#endregion"; // Comparison operators

	_out << sp << nl << "#region Object members";

	_out << sp << nl << "public override int GetHashCode()";
	_out << sb;
	_out << nl << "int __h = 0;";
	for(q = dataMembers.begin(); q != dataMembers.end(); ++q)
	{
	    string memberName = fixKwd((*q)->name());
	    bool isValue = isValueType((*q)->type());
	    if(!isValue)
	    {
		_out << nl << "if(" << memberName << " != null)";
		_out << sb;
	    }
	    _out << nl << "__h = (__h << 3) ^ " << memberName << ".GetHashCode();";
	    if(!isValue)
	    {
		_out << eb;
	    }
	}
	_out << nl << "return __h;";
	_out << eb;

	_out << sp << nl << "public override bool Equals(object other)";
	_out << sb;
	_out << nl << "return CompareTo(other) == 0;";
	_out << eb;

	_out << sp << nl << "public static bool Equals(" << name << " __lhs, " << name << " __rhs)";
	_out << sb;
	_out << nl << "return __lhs == null ? __rhs == null : __lhs.CompareTo(__rhs) == 0;";
	_out << eb;

	_out << sp << nl << "#endregion"; // Object members
    }

    if(!p->isInterface())
    {
	_out << sp << nl << "#endregion"; // class
    }

    _out << eb;
}

void
Slice::Gen::TypesVisitor::visitOperation(const OperationPtr& p)
{
    if(ClassDefPtr::dynamicCast(p->container())->isInterface())
    {
        return;
    }

    string name = fixKwd(p->name());

    _out << sp << nl << "public abstract " << typeToString(p->returnType()) << " " << name << "(";
    ParamDeclList paramList = p->parameters();
    for(ParamDeclList::const_iterator q = paramList.begin(); q != paramList.end(); ++q)
    {
	if(q != paramList.begin())
	{
	    _out << ", ";
	}
	if((*q)->isOutParam())
	{
	    _out << "ref ";
	}
	_out << typeToString((*q)->type()) << " " << fixKwd((*q)->name());
    }
    if(!ClassDefPtr::dynamicCast(p->container())->isLocal())
    {
        if(!paramList.empty())
	{
	    _out << ", ";
	}
	_out << "Ice.Current __current";
    }
    _out << ");";
}

void
Slice::Gen::TypesVisitor::visitSequence(const SequencePtr& p)
{
    string name = fixKwd(fixGlobal(p));
    string s = typeToString(p->type());
    bool isValue = isValueType(p->type());

    _out << sp << nl << "public class " << name
         << " : System.Collections.CollectionBase, System.IComparable";
    _out << sb;

    _out << sp << nl << "#region " << name << " members";

    _out << sp << nl << "#region Indexer";

    _out << nl << "public " << s << " this[int index]";
    _out << sb;
    _out << nl << "get";
    _out << sb;
    _out << nl << "return (" << s << ")List[index];";
    _out << eb;

    _out << nl << "set";
    _out << sb;
    _out << nl << "List[index] = value;";
    _out << eb;
    _out << eb;

    _out << sp << nl << "#endregion"; // Indexer

    _out << sp << nl << "#region ICollectionBase members";

    _out << sp << nl << "public int Add(" << s << " value)";
    _out << sb;
    _out << nl << "return List.Add(value);";
    _out << eb;

    _out << sp << nl << "public int IndexOf(" << s << " value)";
    _out << sb;
    _out << nl << "return List.IndexOf(value);";
    _out << eb;

    _out << sp << nl << "public void Insert(int index, " << s << " value)";
    _out << sb;
    _out << nl << "List.Insert(index, value);";
    _out << eb;

    _out << sp << nl << "public void Remove(" << s << " value)";
    _out << sb;
    _out << nl << "List.Remove(value);";
    _out << eb;

    _out << sp << nl << "public bool Contains(" << s << " value)";
    _out << sb;
    _out << nl << "return List.Contains(value);";
    _out << eb;

    _out << sp << nl << "#endregion"; // ICollectionBase members

    _out << sp << nl << "#region IComparable members";

    _out << sp << nl << "public int CompareTo(object __other)";
    _out << sb;
    _out << nl << "if(__other == null)";
    _out << sb;
    _out << nl << "return 1;";
    _out << eb;
    _out << nl << "if(object.ReferenceEquals(this, __other))";
    _out << sb;
    _out << nl << "return 0;";
    _out << eb;
    _out << nl << "if(!(__other is " << name << "))";
    _out << sb;
    _out << nl << "throw new System.ArgumentException(\"CompareTo: expected argument of type `" << name
         << "'\", \"__other\");";
    _out << eb;
    _out << nl << "int __limit = System.Math.Min(this.Count, ((" << name << ")__other).Count);";
    _out << nl << "for(int __i = 0; __i < __limit; ++__i)";
    _out << sb;
    if(!isValue)
    {
	_out << nl << "if(this[__i] == null && ((" << name << ")__other)[__i] != null)";
	_out << sb;
	_out << nl << "return -1;";
	_out << eb;
    }
    _out << nl << "int __ret = this[__i].CompareTo(((" << name << ")__other)[__i]);";
    _out << nl << "if(__ret != 0)";
    _out << sb;
    _out << nl << "return __ret;";
    _out << eb;
    _out << eb;
    _out << nl << "return this.Count < ((" << name << ")__other).Count ? -1 : ((this.Count > (("
         << name << ")__other).Count) ? 1 : 0);";
    _out << eb;

    _out << sp << nl << "#endregion"; // IComparable members

    _out << sp << nl << "#region Comparison operators";

    _out << sp << nl << "public static bool operator==(" << name << " __lhs, " << name << " __rhs)";
    _out << sb;
    _out << nl << "return Equals(__lhs, __rhs);";
    _out << eb;

    _out << sp << nl << "public static bool operator!=(" << name << " __lhs, " << name << " __rhs)";
    _out << sb;
    _out << nl << "return !Equals(__lhs, __rhs);";
    _out << eb;

    _out << sp << nl << "public static bool operator<(" << name << " __lhs, " << name << " __rhs)";
    _out << sb;
    _out << nl << "return __lhs == null ? __rhs != null : __lhs.CompareTo(__rhs) < 0;";
    _out << eb;

    _out << sp << nl << "public static bool operator>(" << name << " __lhs, " << name << " __rhs)";
    _out << sb;
    _out << nl << "return __lhs == null ? false : __lhs.CompareTo(__rhs) > 0;";
    _out << eb;

    _out << sp << nl << "public static bool operator<=(" << name << " __lhs, " << name << " __rhs)";
    _out << sb;
    _out << nl << "return __lhs == null ? true : __lhs.CompareTo(__rhs) <= 0;";
    _out << eb;

    _out << sp << nl << "public static bool operator>=(" << name << " __lhs, " << name << " __rhs)";
    _out << sb;
    _out << nl << "return __lhs == null ? __rhs == null : __lhs.CompareTo(__rhs) >= 0;";
    _out << eb;

    _out << sp << nl << "#endregion"; // Comparison operators

    _out << sp << nl << "#region Object members";

    _out << sp << nl << "public override int GetHashCode()";
    _out << sb;
    _out << nl << "int hash = 0;";
    _out << nl << "for(int i = 0; i < Count; ++i)";
    _out << sb;
    if(!isValue)
    {
	_out << nl << "if((object)this[i] != null)";
	_out << sb;
    }
    _out << nl << "hash = (hash << 3) ^ this[i].GetHashCode();";
    if(!isValue)
    {
	_out << eb;
    }
    _out << eb;
    _out << nl << "return hash;";
    _out << eb;

    _out << sp << nl << "public override bool Equals(object other)";
    _out << sb;
    _out << nl << "if(Count != ((" << name << ")other).Count)";
    _out << sb;
    _out << nl << "return false;";
    _out << eb;
    _out << nl << "return CompareTo(other) == 0;";
    _out << eb;

    _out << sp << nl << "public static bool Equals(" << name << " __lhs, " << name << " __rhs)";
    _out << sb;
    _out << nl << "return __lhs == null ? __rhs == null : __lhs.Equals(__rhs);";
    _out << eb;

    _out << sp << nl << "#endregion"; // Object members

    _out << sp << nl << "#endregion"; // sequence

    _out << eb;
}

bool
Slice::Gen::TypesVisitor::visitExceptionStart(const ExceptionPtr& p)
{
    string name = fixKwd(fixGlobal(p));
    ExceptionPtr base = p->base();

    _out << sp << nl << "public class " << name << " : ";
    if(base)
    {
        _out << fixKwd(base->scoped());
    }
    else
    {
        _out << (p->isLocal() ? "Ice.LocalException" : "Ice.UserException");
    }
    _out << sb;

    _out << sp << nl << "#region " << name << " members";

    _out << sp << nl << "#region Slice data members";

    return true;
}

void
Slice::Gen::TypesVisitor::visitExceptionEnd(const ExceptionPtr& p)
{
    string name = fixKwd(fixGlobal(p));
    string scoped = fixKwd(p->scoped());

    DataMemberList dataMembers = p->dataMembers();
    DataMemberList::const_iterator q;

    _out << sp << nl << "#endregion"; // Slice data members

    _out << sp << nl << "#region Constructors";

    _out << sp << nl << "public " << name << "() : base(\"" << name << "\")";
    _out << sb;
    _out << eb;

    _out << sp << nl << "public " << name << "(string m) : base(m)";
    _out << sb;
    _out << eb;

    _out << sp << nl << "public " << name << "(string m, System.Exception e) : base(m, e)";
    _out << sb;
    _out << eb;

    _out << sp << nl << "#endregion"; // Constructors

    _out << sp << nl << "#region Comparison operators";

    _out << sp << nl << "public static bool operator==(" << name << " __lhs, " << name << " __rhs)";
    _out << sb;
    _out << nl << "return Equals(__lhs, __rhs);";
    _out << eb;

    _out << sp << nl << "public static bool operator!=(" << name << " __lhs, " << name << " __rhs)";
    _out << sb;
    _out << nl << "return !Equals(__lhs, __rhs);";
    _out << eb;

    _out << sp << nl << "#endregion"; // Comparison operators

    _out << sp << nl << "#region Object members";

    _out << sp << nl << "public override int GetHashCode()";
    _out << sb;
    _out << nl << "int __h = 0;";
    for(q = dataMembers.begin(); q != dataMembers.end(); ++q)
    {
        string memberName = fixKwd((*q)->name());
	bool isValue = isValueType((*q)->type());
	if(!isValue)
	{
	    _out << nl << "if((object)" << memberName << " != null)";
	    _out << sb;
	}
	_out << nl << "__h = (__h << 3) ^ " << memberName << ".GetHashCode();";
	if(!isValue)
	{
	    _out << eb;
	}
    }
    _out << nl << "return __h;";
    _out << eb;

    _out << sp << nl << "public override bool Equals(object __other)";
    _out << sb;
    _out << nl << "if(__other == null)";
    _out << sb;
    _out << nl << "return false;";
    _out << eb;
    _out << nl << "if(object.ReferenceEquals(this, __other))";
    _out << sb;
    _out << nl << "return true;";
    _out << eb;
    _out << nl << "if(!(__other is " << name << "))";
    _out << sb;
    _out << nl << "throw new System.ArgumentException(\"expected argument of type `" << name << "'\", \"__other\");";
    _out << eb;
    for(q = dataMembers.begin(); q != dataMembers.end(); ++q)
    {
        string memberName = fixKwd((*q)->name());
	_out << nl << "if(!" << memberName << ".Equals(((" << name << ")__other)." << memberName << "))";
	_out << sb;
	_out << nl << "return false;";
	_out << eb;
    }
    _out << nl << "return true;";
    _out << eb;

    _out << sp << nl << "public static bool Equals(" << name << " __lhs, " << name << " __rhs)";
    _out << sb;
    _out << nl << "return __lhs == null ? __rhs == null : __lhs.Equals(__rhs);";
    _out << eb;

    _out << sp << nl << "#endregion"; // Object members

    _out << sp << nl << "#endregion"; // exception

    _out << eb;
}

bool
Slice::Gen::TypesVisitor::visitStructStart(const StructPtr& p)
{
    string name = fixKwd(fixGlobal(p));

    _out << sp << nl << "public class " << name << " : System.IComparable";
    _out << sb;

    _out << sp << nl << "#region " << name << " members";

    _out << sp << nl << "#region Slice data members";

    return true;
}

void
Slice::Gen::TypesVisitor::visitStructEnd(const StructPtr& p)
{
    string name = fixKwd(fixGlobal(p));

    DataMemberList dataMembers = p->dataMembers();
    DataMemberList::const_iterator q;

    _out << sp << nl << "#endregion"; // Slice data members

    _out << sp << nl << "#region IComparable members";

    _out << sp << nl << "public int CompareTo(object __other)";
    _out << sb;
    _out << nl << "if(__other == null)";
    _out << sb;
    _out << nl << "return 1;";
    _out << eb;
    _out << nl << "if(object.ReferenceEquals(this, __other))";
    _out << sb;
    _out << nl << "return 0;";
    _out << eb;
    _out << nl << "if(!(__other is " << name << "))";
    _out << sb;
    _out << nl << "throw new System.ArgumentException(\"expected argument of type `" << name << "'\", \"__other\");";
    _out << eb;
    _out << nl << "int __ret;";
    for(q = dataMembers.begin(); q != dataMembers.end(); ++q)
    {
        string memberName = fixKwd((*q)->name());
	if(!isValueType((*q)->type()))
	{
	    _out << nl << "if(" << memberName << " == null && ((" << name << ")__other)." << memberName << " != null)";
	    _out << sb;
	    _out << nl << "return -1;";
	    _out << eb;
	}
	_out << nl << "if((__ret = " << memberName << ".CompareTo(((" << name << ")__other)."
	     << memberName << ")) != 0)";
	_out << sb;
	_out << nl << "return __ret;";
	_out << eb;
    }
    _out << nl << "return 0;";
    _out << eb;

    _out << sp << nl << "#endregion"; // IComparable members

    _out << sp << nl << "#region Comparison operators";

    _out << sp << nl << "public static bool operator==(" << name << " __lhs, " << name << " __rhs)";
    _out << sb;
    _out << nl << "return Equals(__lhs, __rhs);";
    _out << eb;

    _out << sp << nl << "public static bool operator!=(" << name << " __lhs, " << name << " __rhs)";
    _out << sb;
    _out << nl << "return !Equals(__lhs, __rhs);";
    _out << eb;

    _out << sp << nl << "public static bool operator<(" << name << " __lhs, " << name << " __rhs)";
    _out << sb;
    _out << nl << "return __lhs == null ? __rhs != null : __lhs.CompareTo(__rhs) < 0;";
    _out << eb;

    _out << sp << nl << "public static bool operator>(" << name << " __lhs, " << name << " __rhs)";
    _out << sb;
    _out << nl << "return __lhs == null ? false : __lhs.CompareTo(__rhs) > 0;";
    _out << eb;

    _out << sp << nl << "public static bool operator<=(" << name << " __lhs, " << name << " __rhs)";
    _out << sb;
    _out << nl << "return __lhs == null ? true : __lhs.CompareTo(__rhs) <= 0;";
    _out << eb;

    _out << sp << nl << "public static bool operator>=(" << name << " __lhs, " << name << " __rhs)";
    _out << sb;
    _out << nl << "return __lhs == null ? __rhs == null : __lhs.CompareTo(__rhs) >= 0;";
    _out << eb;

    _out << sp << nl << "#endregion"; // Comparison operators

    _out << sp << nl << "#region Object members";

    _out << sp << nl << "public override int GetHashCode()";
    _out << sb;
    _out << nl << "int __h = 0;";
    for(q = dataMembers.begin(); q != dataMembers.end(); ++q)
    {
        string memberName = fixKwd((*q)->name());
	bool isValue = isValueType((*q)->type());
	if(!isValue)
	{
	    _out << nl << "if(" << memberName << " != null)";
	    _out << sb;
	}
	_out << nl << "__h = (__h << 3) ^ " << memberName << ".GetHashCode();";
	if(!isValue)
	{
	    _out << eb;
	}
    }
    _out << nl << "return __h;";
    _out << eb;

    _out << sp << nl << "public override bool Equals(object other)";
    _out << sb;
    _out << nl << "return CompareTo(other) == 0;";
    _out << eb;

    _out << sp << nl << "public static bool Equals(" << name << " __lhs, " << name << " __rhs)";
    _out << sb;
    _out << nl << "return __lhs == null ? __rhs == null : __lhs.CompareTo(__rhs) == 0;";
    _out << eb;

    _out << sp << nl << "#endregion"; // Object members

    _out << sp << nl << "#endregion"; // struct

    _out << eb;
}

void
Slice::Gen::TypesVisitor::visitDictionary(const DictionaryPtr& p)
{
    string name = fixKwd(fixGlobal(p));
    string ks = typeToString(p->keyType());
    string vs = typeToString(p->valueType());
    bool valueIsValue = isValueType(p->valueType());

    _out << sp << nl << "public class " << name
         << " : System.Collections.DictionaryBase, System.IComparable";
    _out << sb;

    _out << sp << nl << "#region " << name << " members";

    _out << sp << nl << "#region " << name << " properties";

    _out << sp << nl << "#region Indexer";

    _out << nl << "public " << vs << " this[" << ks << " key]";
    _out << sb;
    _out << nl << "get";
    _out << sb;
    _out << nl << "return (" << vs << ")Dictionary[key];";
    _out << eb;

    _out << nl << "set";
    _out << sb;
    _out << nl << "Dictionary[key] = value;";
    _out << eb;
    _out << eb;

    _out << sp << nl << "#endregion"; // Indexer

    _out << sp << nl << "public System.Collections.ICollection Keys";
    _out << sb;
    _out << nl << "get";
    _out << sb;
    _out << nl << "return Dictionary.Keys;";
    _out << eb;
    _out << eb;

    _out << sp << nl << "public System.Collections.ICollection Values";
    _out << sb;
    _out << nl << "get";
    _out << sb;
    _out << nl << "return Dictionary.Values;";
    _out << eb;
    _out << eb;

    _out << sp << nl << "#endregion"; // properties

    _out << sp << nl << "#region IDictionary members";

    _out << sp << nl << "public void Add(" << ks << " key, " << vs << " value)";
    _out << sb;
    _out << nl << "Dictionary[key] = value;";
    _out << eb;

    _out << sp << nl << "public void Remove(" << ks << " key)";
    _out << sb;
    _out << nl << "Dictionary.Remove(key);";
    _out << eb;

    _out << sp << nl << "public bool Contains(" << ks << " key)";
    _out << sb;
    _out << nl << "return Dictionary.Contains(key);";
    _out << eb;

    _out << sp << nl << "#endregion"; // IDictionary members

    _out << sp << nl << "#region IComparable members";

    _out << sp << nl << "public int CompareTo(object __other)";
    _out << sb;
    _out << nl << "if(__other == null)";
    _out << sb;
    _out << nl << "return 1;";
    _out << eb;
    _out << nl << "if(object.ReferenceEquals(this, __other))";
    _out << sb;
    _out << nl << "return 0;";
    _out << eb;
    _out << nl << "if(!(__other is " << name << "))";
    _out << sb;
    _out << nl << "throw new System.ArgumentException(\"CompareTo: expected argument of type `" << name
         << "'\", \"__other\");";
    _out << eb;
    _out << nl << ks << "[] __klhs = new " << ks << "[Count];";
    _out << nl << "Keys.CopyTo(__klhs, 0);";
    _out << nl << "System.Array.Sort(__klhs);";
    _out << nl << ks << "[] __krhs = new " << ks << "[((" << name << ")__other).Count];";
    _out << nl << "((" << name << ")__other).Keys.CopyTo(__krhs, 0);";
    _out << nl << "System.Array.Sort(__krhs);";
    _out << nl << "int __limit = System.Math.Min(__klhs.Length, __krhs.Length);";
    _out << nl << "for(int i = 0; i < __limit; ++i)";
    _out << sb;
    _out << nl << "int ret = __klhs[i].CompareTo(__krhs[i]);";
    _out << nl << "if(ret != 0)";
    _out << sb;
    _out << nl << "return ret;";
    _out << eb;
    _out << eb;
    _out << nl << vs << "[] __vlhs = new " << vs << "[Count];";
    _out << nl << "Values.CopyTo(__vlhs, 0);";
    _out << nl << "System.Array.Sort(__vlhs);";
    _out << nl << vs << "[] __vrhs = new " << vs << "[((" << name << ")__other).Count];";
    _out << nl << "((" << name << ")__other).Values.CopyTo(__vrhs, 0);";
    _out << nl << "System.Array.Sort(__vrhs);";
    _out << nl << "for(int i = 0; i < __limit; ++i)";
    _out << sb;
    if(!valueIsValue)
    {
	_out << nl << "if(__vlhs[i] == null && __vrhs[i] != null)";
	_out << sb;
	_out << nl << "return -1;";
	_out << eb;
    }
    _out << nl << "int ret = __vlhs[i].CompareTo(__vrhs[i]);";
    _out << nl << "if(ret != 0)";
    _out << sb;
    _out << nl << "return ret;";
    _out << eb;
    _out << eb;
    _out << nl << "return __klhs.Length < __krhs.Length ? -1 : (__klhs.Length > __krhs.Length ? 1 : 0);";
    _out << eb;

    _out << sp << nl << "#endregion"; // IComparable members

    _out << sp << nl << "#region Comparison operators";

    _out << sp << nl << "public static bool operator==(" << name << " __lhs, " << name << " __rhs)";
    _out << sb;
    _out << nl << "return Equals(__lhs, __rhs);";
    _out << eb;

    _out << sp << nl << "public static bool operator!=(" << name << " __lhs, " << name << " __rhs)";
    _out << sb;
    _out << nl << "return !Equals(__lhs, __rhs);";
    _out << eb;

    _out << sp << nl << "public static bool operator<(" << name << " __lhs, " << name << " __rhs)";
    _out << sb;
    _out << nl << "return __lhs == null ? __rhs != null : __lhs.CompareTo(__rhs) < 0;";
    _out << eb;

    _out << sp << nl << "public static bool operator>(" << name << " __lhs, " << name << " __rhs)";
    _out << sb;
    _out << nl << "return __lhs == null ? false : __lhs.CompareTo(__rhs) > 0;";
    _out << eb;

    _out << sp << nl << "public static bool operator<=(" << name << " __lhs, " << name << " __rhs)";
    _out << sb;
    _out << nl << "return __lhs == null ? true : __lhs.CompareTo(__rhs) <= 0;";
    _out << eb;

    _out << sp << nl << "public static bool operator>=(" << name << " __lhs, " << name << " __rhs)";
    _out << sb;
    _out << nl << "return __lhs == null ? __rhs == null : __lhs.CompareTo(__rhs) >= 0;";
    _out << eb;

    _out << sp << nl << "#endregion"; // Comparison operators

    _out << sp << nl << "#region Object members";

    _out << sp << nl << "public override int GetHashCode()";
    _out << sb;
    _out << nl << "int hash = 0;";
    _out << nl << "foreach(System.Collections.DictionaryEntry e in Dictionary)";
    _out << sb;
    _out << nl << "hash = (hash << 3) ^ e.Key.GetHashCode();";
    if(!valueIsValue)
    {
	_out << nl << "if(e.Value != null)";
	_out << sb;
    }
    _out << nl << "hash = (hash << 3) ^ e.Value.GetHashCode();";
    if(!valueIsValue)
    {
	_out << eb;
    }
    _out << eb;
    _out << nl << "return hash;";
    _out << eb;

    _out << sp << nl << "public override bool Equals(object other)";
    _out << sb;
    _out << nl << "if(Count != ((" << name << ")other).Count)";
    _out << sb;
    _out << nl << "return false;";
    _out << eb;
    _out << nl << "return CompareTo(other) == 0;";
    _out << eb;

    _out << sp << nl << "public static bool Equals(" << name << " __lhs, " << name << " __rhs)";
    _out << sb;
    _out << nl << "return __lhs == null ? __rhs == null : __lhs.Equals(__rhs);";
    _out << eb;

    _out << sp << nl << "#endregion"; // Object members

    _out << sp << nl << "#endregion"; // dictionary

    _out << eb;
}

void
Slice::Gen::TypesVisitor::visitEnum(const EnumPtr& p)
{
    string name = fixKwd(fixGlobal(p));
    EnumeratorList enumerators = p->getEnumerators();
    _out << sp << nl << "public enum " << name;
    _out << sb;
    EnumeratorList::const_iterator en = enumerators.begin();
    while(en != enumerators.end())
    {
        _out << nl << fixKwd((*en)->name());
	if(++en != enumerators.end())
	{
	    _out << ',';
	}
    }
    _out << eb;
}

void
Slice::Gen::TypesVisitor::visitConst(const ConstPtr& p)
{
    string name = fixKwd(fixGlobal(p));
    _out << sp << nl << "public class " << name;
    _out << sb;
    _out << sp << nl << "public const " << typeToString(p->type()) << " value = ";
    BuiltinPtr bp = BuiltinPtr::dynamicCast(p->type());
    if(bp && bp->kind() == Builtin::KindString)
    {
	//
	// Expand strings into the basic source character set. We can't use isalpha() and the like
	// here because they are sensitive to the current locale.
	//
	static const string basicSourceChars = "abcdefghijklmnopqrstuvwxyz"
					       "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
					       "0123456789"
					       "_{}[]#()<>%:;,?*+=/^&|~!=,\\\"' \t";
    	static const set<char> charSet(basicSourceChars.begin(), basicSourceChars.end());

	_out << "\"";					 // Opening "

	ios_base::fmtflags originalFlags = _out.flags(); // Save stream state
	streamsize originalWidth = _out.width();
	ostream::char_type originalFill = _out.fill();

	const string val = p->value();
	for(string::const_iterator c = val.begin(); c != val.end(); ++c)
	{
	    if(charSet.find(*c) == charSet.end())
	    {
		unsigned char uc = *c;			 // char may be signed, so make it positive
		_out << "\\u";    			 // Print as unicode if not in basic source character set
		_out.flags(ios_base::hex);
		_out.width(4);
		_out.fill('0');
		_out << static_cast<unsigned>(uc);
	    }
	    else
	    {
		_out << *c;				 // Print normally if in basic source character set
	    }
	}

	_out.fill(originalFill);			 // Restore stream state
	_out.width(originalWidth);
	_out.flags(originalFlags);

	_out << "\"";					 // Closing "
    }
    else if(bp && bp->kind() == Builtin::KindLong)
    {
	_out << p->value() << "L";
    }
    else if(bp && bp->kind() == Builtin::KindFloat)
    {
	_out << p->value() << "F";
    }
    else
    {
	EnumPtr ep = EnumPtr::dynamicCast(p->type());
	if(ep)
	{
	    _out << fixKwd(typeToString(p->type())) << "." << fixKwd(p->value());
	}
	else
	{
	    _out << p->value();
	}
    }
    _out << ";";
    _out << eb;
}

void
Slice::Gen::TypesVisitor::visitDataMember(const DataMemberPtr& p)
{
    _out << sp << nl << "public " << typeToString(p->type()) << " " << fixKwd(p->name()) << ";";
}

Slice::Gen::ProxyVisitor::ProxyVisitor(IceUtil::Output& out) :
    CsVisitor(out)
{
}

bool
Slice::Gen::ProxyVisitor::visitModuleStart(const ModulePtr& p)
{
    string name = fixKwd(fixGlobal(p));
    _out << sp << nl << "namespace " << name;
    _out << sb;
    _out << nl << "#region " << name << " members";

    return true;
}

void
Slice::Gen::ProxyVisitor::visitModuleEnd(const ModulePtr& p)
{
    _out << sp << nl << "#endregion"; // module
    _out << eb;
}

bool
Slice::Gen::ProxyVisitor::visitClassDefStart(const ClassDefPtr& p)
{
    if(p->isLocal())
    {
	return false;
    }

    string name = fixKwd(fixGlobal(p));
    string scoped = fixKwd(p->scoped());
    ClassList bases = p->bases();

    _out << sp << nl << "public interface " << name << "Prx : ";
    if(bases.empty())
    {
        _out << "Ice.ObjectPrx";
    }
    else
    {
        _out.useCurrentPosAsIndent();
	ClassList::const_iterator q = bases.begin();
	while(q != bases.end())
	{
	    _out << fixKwd((*q)->scoped()) << "Prx";
	    if(++q != bases.end())
	    {
		_out << ',' << nl;
	    }
	}
	_out.restoreIndent();
    }

    _out << sb;

    _out << sp << nl << "#region " << name << "Prx members";

    return true;
}

void
Slice::Gen::ProxyVisitor::visitClassDefEnd(const ClassDefPtr& p)
{

    _out << sp << nl << "#endregion"; // Prx members

    _out << eb;
}

void
Slice::Gen::ProxyVisitor::visitOperation(const OperationPtr& p)
{
    string name = fixKwd(p->name());
    ParamDeclList paramList = p->parameters();
    ParamDeclList::const_iterator q;

    _out << sp << nl << typeToString(p->returnType()) << " " << name << "(";
    for(q = paramList.begin(); q != paramList.end(); ++q)
    {
	if(q != paramList.begin())
	{
	    _out << ", ";
	}
	if((*q)->isOutParam())
	{
	    _out << "ref ";
	}
	_out << typeToString((*q)->type()) << " " << fixKwd((*q)->name());
    }
    _out << ");";

    _out << nl << typeToString(p->returnType()) << " " << name << "(";
    for(q = paramList.begin(); q != paramList.end(); ++q)
    {
	if(q != paramList.begin())
	{
	    _out << ", ";
	}
	if((*q)->isOutParam())
	{
	    _out << "ref ";
	}
	_out << typeToString((*q)->type()) << " " << fixKwd((*q)->name());
    }
    if(!paramList.empty())
    {
        _out << ", ";
    }
    _out << "System.Collections.Hashtable __context";
    _out << ");";
}
