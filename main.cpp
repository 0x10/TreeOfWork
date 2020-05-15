#include "tree_of_work.h"

#include <iostream>




static void w1_func( const TreeOfWork::Work::Control& work_control )
{
    std::this_thread::sleep_for( std::chrono::seconds(1) );

    std::cout << "W1: Done!\n";
    work_control.set_completed();
}

class Worker2
{
public:

    void w2_func( const TreeOfWork::Work::Control& work_control )
    {
        static int counter = 0;

        while ( ++counter <= 100 )
        {
            std::cout << "W2: " << counter << std::endl;
        }

        std::cout << "W2: Done!\n";
        work_control.set_completed();
    }
};


int main()
{
    auto W0 = TreeOfWork::Work::make_empty_root();

    auto W1 = std::make_shared<TreeOfWork::Work>( std::bind( &w1_func, std::placeholders::_1 ) );
    auto W2 = std::make_shared<TreeOfWork::Work>( std::bind( &Worker2::w2_func, Worker2(), std::placeholders::_1 ) );
    auto W3 = std::make_shared<TreeOfWork::Work>( [](const TreeOfWork::Work::Control& work_control)
                                                  {
                                                    std::cout << "work completed, exit in 2s" << std::endl;
                                                    std::this_thread::sleep_for( std::chrono::seconds(2) );
                                                    work_control.set_completed();
                                                  } );

    TreeOfWork::Work::execute_if_any_finished( {W0}, {W1, W2} );
    TreeOfWork::Work::execute_if_all_finished( {W1, W2}, {W3} );

    W0->trigger();
    
    W3->wait_for_done();
    W3->wait_for_done();

    std::cout << "..Done" << std::endl;
    return 0;
}
