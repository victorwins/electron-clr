#include "node-clr.h"

using namespace v8;
using namespace System::Collections::Generic;
using namespace System::IO;
using namespace System::Linq;
using namespace System::Reflection;

static Handle<Value> Import(const Arguments &args)
{
	HandleScope scope;

	if (args.Length() != 1 || !args[0]->IsString())
	{
		ThrowException(Exception::TypeError(String::New("Argument error")));
		return scope.Close(Undefined());
	}

	auto name = CLRString(args[0]);
	Assembly^ assembly;
	try
	{
		if (File::Exists(name))
		{
			assembly = Assembly::LoadFrom(name);
		}
		else
		{
#pragma warning(push)
#pragma warning(disable:4947)
			assembly = Assembly::LoadWithPartialName(name);
#pragma warning(pop)
		}
	}
	catch (System::Exception ^ex)
	{
		ThrowException(V8Exception(ex));
		return scope.Close(Undefined());
	}

	if (assembly == nullptr)
	{
		ThrowException(Exception::Error(String::New("Assembly not found")));
		return scope.Close(Undefined());
	}

	return scope.Close(Undefined());
}

static Handle<Value> GetAssemblies(const Arguments& args)
{
	HandleScope scope;

	if (args.Length() != 0)
	{
		ThrowException(Exception::TypeError(String::New("Argument error")));
		return scope.Close(Undefined());
	}

	auto arr = Array::New();
	auto index = 0;
	auto assemblies = System::AppDomain::CurrentDomain->GetAssemblies();
	for (int i = 0; i < assemblies->Length; i++)
	{
		auto assembly = assemblies[i];
		if (assembly == System::Reflection::Assembly::GetExecutingAssembly())
		{
			continue;
		}

		arr->Set(Number::New(index++), V8String(assembly->FullName));
	}

	return scope.Close(arr);
}

static Handle<Value> GetTypes(const Arguments& args)
{
	HandleScope scope;

	if (args.Length() != 0)
	{
		ThrowException(Exception::TypeError(String::New("Argument error")));
		return scope.Close(Undefined());
	}

	auto arr = Array::New();
	auto index = 0;
	auto assemblies = System::AppDomain::CurrentDomain->GetAssemblies();
	for (int i = 0; i < assemblies->Length; i++)
	{
		auto assembly = assemblies[i];
		if (assembly == System::Reflection::Assembly::GetExecutingAssembly())
		{
			continue;
		}

		auto types = assembly->GetTypes();
		for (int j = 0; j < types->Length; j++)
		{
			auto type = types[j];
			if (!type->IsPublic)
			{
				continue;
			}
			if (type->IsSpecialName)
			{
				continue;
			}

			arr->Set(Number::New(index++), V8String(type->AssemblyQualifiedName));
		}
	}

	return scope.Close(arr);
}

static Handle<Value> CreateConstructor(const Arguments &args)
{
	return CLRObject::CreateConstructor(args);
}

static Handle<Value> GetMembers(const Arguments& args, MemberTypes types)
{
	HandleScope scope;

	if (args.Length() != 2 || !args[0]->IsString())
	{
		ThrowException(Exception::TypeError(String::New("Argument error")));
		return scope.Close(Undefined());
	}
	
	auto isStatic = !args[1]->BooleanValue();
	System::Type^ type;
	try
	{
		type = System::Type::GetType(CLRString(args[0]));
	}
	catch (System::Exception^ ex)
	{
		ThrowException(V8Exception(ex));
		return scope.Close(Undefined());
	}

	auto members = type->GetMembers(
		((isStatic) ? BindingFlags::Static : BindingFlags::Instance) |
		BindingFlags::Public |
		BindingFlags::FlattenHierarchy);
	auto names = gcnew List<System::String^>();
	for (int i = 0; i < members->Length; i++)
	{
		auto member = members[i];

		if ((int)(member->MemberType & types) == 0)
		{
			continue;
		}
		
		auto ei = dynamic_cast<EventInfo^>(member);
		if (ei != nullptr && ei->IsSpecialName)
		{
			continue;
		}
		auto fi = dynamic_cast<FieldInfo^>(member);
		if (fi != nullptr && fi->IsSpecialName)
		{
			continue;
		}
		auto mi = dynamic_cast<MethodBase^>(member);
		if (mi != nullptr && mi->IsSpecialName)
		{
			continue;
		}
		auto pi = dynamic_cast<PropertyInfo^>(member);
		if (pi != nullptr && pi->IsSpecialName)
		{
			continue;
		}

		names->Add(member->Name);
	}
	names = Enumerable::ToList(Enumerable::Distinct(names));

	auto arr = Array::New();
	for (int i = 0; i < names->Count; i++)
	{
		arr->Set(Number::New(i), V8String(names[i]));
	}

	return scope.Close(arr);
}

static Handle<Value> InvokeMember(const Arguments &args, BindingFlags attr)
{
	HandleScope scope;

	if (args.Length() != 4 ||
		!args[0]->IsString() ||
		(!CLRObject::IsWrapped(args[1]) && !args[1]->IsNull()) ||
		!args[2]->IsString() ||
		!args[3]->IsArray())
	{
		ThrowException(Exception::TypeError(String::New("Argument error")));
		return scope.Close(Undefined());
	}

	auto result = V8Binder::InvokeMember(
		Handle<String>::Cast(args[0]),
		Handle<String>::Cast(args[2]),
		attr,
		args[1],
		Handle<Array>::Cast(args[3]));

	return scope.Close(result);
}

static Handle<Value> GetMethods(const Arguments &args)
{
	return GetMembers(args, MemberTypes::Method);
}

static Handle<Value> InvokeMethod(const Arguments &args)
{
	return InvokeMember(args, BindingFlags::InvokeMethod);
}

static Handle<Value> GetProperties(const Arguments &args)
{
	return GetMembers(args, MemberTypes::Field | MemberTypes::Property);
}

static Handle<Value> InvokeGetter(const Arguments& args)
{
	return InvokeMember(args, BindingFlags::GetField | BindingFlags::GetProperty);
}

static Handle<Value> InvokeSetter(const Arguments& args)
{
	return InvokeMember(args, BindingFlags::SetField | BindingFlags::SetProperty);
}

static Handle<Value> IsCLRObject(const Arguments& args)
{
	HandleScope scope;

	if (args.Length() != 1)
	{
		ThrowException(Exception::TypeError(String::New("Argument error")));
		return scope.Close(Undefined());
	}

	return scope.Close(Boolean::New(CLRObject::IsWrapped(args[0])));
}

static void Init(Handle<Object> exports)
{
	exports->Set(String::NewSymbol("import"), FunctionTemplate::New(Import)->GetFunction());
	exports->Set(String::NewSymbol("assemblies"), FunctionTemplate::New(GetAssemblies)->GetFunction());
	exports->Set(String::NewSymbol("types"), FunctionTemplate::New(GetTypes)->GetFunction());
	exports->Set(String::NewSymbol("constructor"), FunctionTemplate::New(CreateConstructor)->GetFunction());
	exports->Set(String::NewSymbol("methods"), FunctionTemplate::New(GetMethods)->GetFunction());
	exports->Set(String::NewSymbol("properties"), FunctionTemplate::New(GetProperties)->GetFunction());
	exports->Set(String::NewSymbol("invoke"), FunctionTemplate::New(InvokeMethod)->GetFunction());
	exports->Set(String::NewSymbol("get"), FunctionTemplate::New(InvokeGetter)->GetFunction());
	exports->Set(String::NewSymbol("set"), FunctionTemplate::New(InvokeSetter)->GetFunction());
	exports->Set(String::NewSymbol("isCLRObject"), FunctionTemplate::New(IsCLRObject)->GetFunction());
}

NODE_MODULE(clr, Init);