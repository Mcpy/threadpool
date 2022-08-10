#include"ThreadPool.h"

int main()
{
	ThreadPool t(5, 10, 10, 60);
	ThreadTask tt[6];
	for (auto& i : tt)
	{
		t.pushTask(&i);
	}
	std::this_thread::sleep_for(std::chrono::seconds(2));
}