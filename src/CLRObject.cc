#include "node-clr.h"

using namespace v8;


Nan::Persistent<v8::ObjectTemplate> CLRObject::objectTemplate_;

void CLRObject::Init()
{
	auto tmpl = Nan::New<ObjectTemplate>();
	tmpl->SetInternalFieldCount(1);
	objectTemplate_.Reset(tmpl);
}

v8::Local<v8::Value> GetPrivate(v8::Local<v8::Object> object,
                                  v8::Local<v8::String> key) {
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    v8::Local<v8::Private> privateKey = v8::Private::ForApi(isolate, key);
    v8::Local<v8::Value> value;
    v8::Maybe<bool> result = object->HasPrivate(context, privateKey);
    if (!(result.IsJust() && result.FromJust()))
      return v8::Local<v8::Value>();
    if (object->GetPrivate(context, privateKey).ToLocal(&value))
      return value;
    return v8::Local<v8::Value>();
  }

  void SetPrivate(v8::Local<v8::Object> object,
                  v8::Local<v8::String> key,
                  v8::Local<v8::Value> value) {
    if (value.IsEmpty())
      return;
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    v8::Local<v8::Private> privateKey = v8::Private::ForApi(isolate, key);
    object->SetPrivate(context, privateKey, value);
  }

bool CLRObject::IsCLRObject(Local<Value> value)
{
	if (!value.IsEmpty() && value->IsObject() && !value->IsFunction())
	{
	    auto type = GetPrivate(Local<Object>::Cast(value), Nan::New<String>("clr::type").ToLocalChecked());
		return !type.IsEmpty();
	}
	else
	{
		return false;
	}
}

Local<Value> CLRObject::GetType(Local<Value> value)
{
	return GetPrivate(Local<Object>::Cast(value), Nan::New<String>("clr::type").ToLocalChecked());
}

bool CLRObject::IsCLRConstructor(Local<Value> value)
{
	if (!value.IsEmpty() && value->IsFunction())
	{
        auto type = GetPrivate(value->ToObject(), Nan::New<String>("clr::type").ToLocalChecked());
		return !type.IsEmpty();
	}
	else
	{
		return false;
	}
}

Local<Value> CLRObject::TypeOf(Local<Value> value)
{
	return GetPrivate(Local<Object>::Cast(value), Nan::New<String>("clr::type").ToLocalChecked());
}

Local<Object> CLRObject::Wrap(Local<Object> obj, System::Object^ value)
{
	auto wrapper = new CLRObject(value);
	wrapper->node::ObjectWrap::Wrap(obj);
	
	auto name = (value != nullptr)
		? ToV8String(value->GetType()->AssemblyQualifiedName)
		: ToV8String(System::Object::typeid->AssemblyQualifiedName);

	SetPrivate(obj,
		Nan::New<String>("clr::type").ToLocalChecked(),
		name);

	return obj;
}

Local<Object> CLRObject::Wrap(System::Object^ value)
{
	auto tmpl = Nan::New<ObjectTemplate>(objectTemplate_);
	auto obj = tmpl->NewInstance();
	return Wrap(obj, value);
}

System::Object^ CLRObject::Unwrap(Local<Value> obj)
{
	if (!IsCLRObject(obj))
	{
		throw gcnew System::ArgumentException("argument \"obj\" is not CLR-wrapped object");
	}

	auto wrapper = node::ObjectWrap::Unwrap<CLRObject>(obj->ToObject());
	return wrapper->value_;
}

Local<Function> CLRObject::CreateConstructor(Local<String> typeName, Local<Function> initializer)
{
	auto type = System::Type::GetType(ToCLRString(typeName), true);

    auto data = Nan::New<v8::Object>();
	SetPrivate(data, Nan::New<String>("clr::type").ToLocalChecked(), ToV8String(type->AssemblyQualifiedName));
	SetPrivate(data, Nan::New<String>("clr::initializer").ToLocalChecked(), initializer);

	auto tpl = Nan::New<FunctionTemplate>(New, data);
	tpl->SetClassName(ToV8String(type->Name));
	tpl->InstanceTemplate()->SetInternalFieldCount(1);
	
	auto ctor = tpl->GetFunction();
	SetPrivate(ctor, Nan::New<String>("clr::type").ToLocalChecked(), ToV8String(type->AssemblyQualifiedName));

	return ctor;
}

NAN_METHOD(CLRObject::New)
{
	Nan::HandleScope scope;

	if (!info.IsConstructCall())
	{
		return Nan::ThrowError("Illegal invocation");
	}

	auto ctor = Local<Object>::Cast(info.Data());
	auto typeName = GetPrivate(ctor, Nan::New<String>("clr::type").ToLocalChecked());

	auto arr = Nan::New<Array>();
	for (int i = 0; i < info.Length(); i++)
	{
		arr->Set(Nan::New<Number>(i), info[i]);
	}
	
	System::Object^ value;
	try
	{
		value = CLRBinder::InvokeConstructor(typeName, arr);
	}
	catch (System::Exception^ ex)
	{
		Nan::ThrowError(ToV8Error(ex));
		return;
	}
	Wrap(info.This(), value);

	auto initializer = GetPrivate(ctor, Nan::New<String>("clr::initializer").ToLocalChecked());
	if (!initializer.IsEmpty())
	{
		std::vector<Local<Value> > params;
		for (int i = 0; i < info.Length(); i++)
		{
			params.push_back(info[i]);
		}
		Local<Function>::Cast(initializer)->Call(info.This(), info.Length(), (0 < params.size()) ? &(params[0]) : nullptr);
	}
}

CLRObject::CLRObject(System::Object^ value)
	: value_(value)
{
}

CLRObject::~CLRObject()
{
}