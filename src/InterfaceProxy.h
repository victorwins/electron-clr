#ifndef INTERFACEPROXY_H_
#define INTERFACEPROXY_H_

#include <msclr\marshal_cppstd.h>
#include <stdlib.h>

using namespace System::Collections::Generic;
using namespace System::Reflection;
using namespace System::Runtime::Remoting::Proxies;
using namespace System::Runtime::Remoting::Messaging;

public ref class InterfaceProxy : public RealProxy
{
    private:
     System::Type^ _type;
     std::map<std::string, V8Function*>* _instance;

    public:
    InterfaceProxy(System::Type^ type, std::map<std::string, V8Function*>* instance) : RealProxy(type)
    {
        _type = type;
        _instance = instance;
    }

    !InterfaceProxy() {
        if(_instance) {
            for(auto it: *_instance) {
                it.second->Destroy();
            }
            delete _instance;
            _instance = NULL;
        }
    }

    ~InterfaceProxy() {
        this->!InterfaceProxy();
    }

    virtual IMessage^ Invoke(IMessage^ msg) override
    {
        auto methodCall = (IMethodCallMessage^)msg;
        auto method = (MethodInfo^)methodCall->MethodBase;

        try
        {
            System::Console::WriteLine("{0}", method);
            if(method->Name == "GetType")
            {
                return gcnew ReturnMessage(_type, nullptr, 0, methodCall->LogicalCallContext, methodCall);
            }
            msclr::interop::marshal_context context;
            std::string name = context.marshal_as<std::string>(method->Name);
            auto found = _instance->find(name);
            if (found != _instance->end())
            {
                auto result = found->second->Invoke(methodCall->InArgs, method->ReturnType);
                System::Console::WriteLine("call {0} => {1}", methodCall->InArgs, result);
                return gcnew ReturnMessage(result, nullptr, 0, methodCall->LogicalCallContext, methodCall);
            }
            else
            {
                throw gcnew System::MissingMemberException(method->Name);
            }
        }
        catch (System::Exception^ e)
        {
            System::Console::WriteLine("failed call {0} => {1}", methodCall->InArgs, e);
            if ( dynamic_cast<TargetInvocationException^>(e) != nullptr && e->InnerException != nullptr)
            {
                return gcnew ReturnMessage(e->InnerException, dynamic_cast<IMethodCallMessage^>(msg));
            }

            return gcnew ReturnMessage(e, dynamic_cast<IMethodCallMessage^>(msg));
        }
    }
};

#endif
