#include"ThreadPool.h"

int textfunc(int a, int b)
{
	return a + b;
}

void texterror()
{
	throw("error");
}

class textclass
{
public:
	int a;
	textclass(int a) :a(a) {};
	int add(int b)
	{
		return a + b;
	}
	int operator()(int b)
	{
		return a + b;
	}
};

int main()
{
	ThreadPool t(5, 10, 10, 60);
	//匿名函数
	auto return_info1 = t.pushTask([](int a) ->int {return a; }, 1);
	//普通函数
	auto return_info2 = t.pushTask(textfunc, 1, 2);
	textclass tc(1);
	//仿函数
	auto return_info3 = t.pushTask(tc, 2);
	//异常处理演示
	auto return_info4 = t.pushTask(texterror);
	//类成员函数
	auto return_info5 = t.pushTask(&textclass::add, &tc, 2);
	std::cout << "return1:" << return_info1.get() << std::endl;
	std::cout << "return2:" << return_info2.get() << std::endl;
	std::cout << "return3:" << return_info3.get() << std::endl;
	std::cout << "return5:" << return_info5.get() << std::endl;
	try
	{
		return_info4.get();
	}
	catch (const char* msg)
	{
		std::cout << "return4 exception:" << msg << std::endl;
	}
	std::this_thread::sleep_for(std::chrono::seconds(2));
}