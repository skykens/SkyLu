/*
 * 线程池的单元测试

 */


#include <gtest/gtest.h>
#include <numeric>
#include <vector>
TEST(MyTest, Sum)
{

ThreadPool pool;
pool.start();
long long count = 0;
while(count+1){
pool.addTask([&count](){
int i = 0;
printf("runing...\n");
while(i<5){
count++;
i++;
}

});
int i = 0;
if(i++%5) sleep(1);
printf("main runing\n");
}
}
int main(int argc, char *argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}





