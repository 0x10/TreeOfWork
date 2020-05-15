/********************************************************************************
 * MIT License
 *
 * Copyright (c) 2020 Christian Kranz
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *******************************************************************************/
#ifndef TREE_OF_WORK
#define TREE_OF_WORK

#include <functional>
#include <thread>
#include <future>
#include <memory>
#include <vector>
#include <chrono>

namespace TreeOfWork
{
/**************************
 * The Tree of Work is a threading concept
 * where each thread is a node within a tree.
 * 
 * Each node has a set of parents and a set of children.
 * 
 * A child is triggered either if all parents are successfully done,
 * or if one of the parents is successfully done.
 *
 * The Work class represents the structure for one node.
 *
 *************************/
class Work
{
    using WorkerSet = std::vector<std::shared_ptr<Work>>;
public:
    /**************************
     * Control structure accessible by the work function
     * to control internal work state and further processing steps
     * (if childs can start or not)
     *************************/
    struct Control
    {
        using NotifyFunc = std::function<void(void)>;

        NotifyFunc          set_completed;
        NotifyFunc          set_failed;
    };
    /**************************
     * the work function definition
     *
     *************************/
    using WorkerFunc = std::function<void(const Work::Control&)>;
    /**************************
     *
     *************************/
    enum class State
    {
        Created,
        Running,
        Completed,
        Failed
    };
    /**************************
     * Support type for the trigger condition
     *   Conditional::OR - execute if any of the parents is done
     *   Conditional::AND - execute if all of the parents are done
     *************************/
    enum class Conditional
    {
        OR,
        AND
    };

public:
    /**************************
     * construct an AND relationship between given sets of work nodes
     *************************/
    static void execute_if_all_finished( const WorkerSet& parents, 
                                         const WorkerSet& children )
    {
        for( auto parent : parents )
        {
            for( auto child : children )
            {
                child->set_trigger_condition( Work::Conditional::AND );
                parent->register_child( child );
            }
        }
    }
    /**************************
     * construct an OR relationship between given sets of work nodes
     *************************/
    static void execute_if_any_finished( const WorkerSet& parents, 
                                         const WorkerSet& children )
    {
        for( auto parent : parents )
        {
            for( auto child : children )
            {
                child->set_trigger_condition( Work::Conditional::OR );
                parent->register_child( child );
            }
        }
    }
    /**************************
     * creates an empty always true node
     *************************/
    static std::shared_ptr<Work> make_empty_root()
    {
        std::shared_ptr<Work> new_root = std::make_shared<Work>( 
                                            [](const Work::Control& control)
                                            { 
                                                control.set_completed(); 
                                            } );
        return new_root;
    }

public:
    /**************************
     * Work is defined by the worker function
     *************************/
    Work( const WorkerFunc& f )
        : m_state{ Work::State::Created }
        , m_control{ std::bind( &Work::done, this, Work::State::Completed ), 
                     std::bind( &Work::done, this, Work::State::Failed ) }
        , m_children()
        , m_worker( f )
        , m_promise_done()
        , m_is_done( m_promise_done.get_future() )
        , m_parent_count(0)
        , m_parent_done_count(0)
        , m_trigger_condition( Work::Conditional::OR )
    {}
    /**************************
     *
     *************************/
    ~Work()
    {}
    /**************************
     * trigger is the precondition hook.
     * if the node is root node, simply call it without arguments
     *
     * starts the worker thread if all/any parents (if any) completed
     * their work successfully
     *************************/
    void trigger( const Work::State parent_state = Work::State::Completed )
    {
        if ( m_state.load() == Work::State::Created )
        {
            bool run_now = false;
            if ( parent_state == Work::State::Completed )
            {
                if ( m_parent_count > 0 )
                    m_parent_count--;

                switch( m_trigger_condition )
                {
                    default: break;
                    case Work::Conditional::OR:
                        run_now = true;
                        break;
                    case Work::Conditional::AND:
                        run_now = m_parent_count == 0;
                        break;
                }

                if ( run_now )
                {
                    m_state = Work::State::Running;

                    std::thread t = std::thread( m_worker, m_control );
                    t.detach();
                }
            }
        }
    }
    /**************************
     * append a new child which is called as soon as the current
     * node is finished
     *************************/
    void register_child( const std::shared_ptr<Work>& child )
    {
        m_children.push_back( child );

        // parent added
        m_parent_count++;
        m_parent_done_count = m_parent_count;
    }
    /**************************
     * change trigger condition
     * the trigger condition defines the behavior of the trigger
     * function.
     * @see Work::Conditional
     *************************/
    void set_trigger_condition( Work::Conditional c )
    {
        m_trigger_condition = c;
    }
    /**************************
     * reset internal state for another run
     *  set deep == true for recursive reset
     *************************/
    void reset( bool deep=false )
    {
        if ( m_state.load() == Work::State::Running )
            wait_for_done();
        
        m_state = Work::State::Created;
        m_promise_done = std::promise<bool>();
        m_is_done = m_promise_done.get_future();
        m_parent_done_count = m_parent_count;
        if ( deep )
        {
            for( std::shared_ptr<Work>& child : m_children )
            {
                if ( child != nullptr )
                    child->reset( deep );
            }
        }
    }
    /**************************
     * blocks until the work is done;
     *************************/
    void wait_for_done()
    {
        if ( m_is_done.valid() && m_is_done.wait_for(std::chrono::seconds(0)) != std::future_status::ready )
            m_is_done.get();
    }

private:
    /**************************
     *
     *************************/
    void done( const Work::State result )
    {
        m_state = result;
        m_promise_done.set_value( true );
        for( std::shared_ptr<Work>& child : m_children )
        {
            if ( child != nullptr )
                child->trigger( m_state.load() );
        }
    }

private:
    std::atomic<Work::State> m_state;
    Work::Control            m_control;
    Work::WorkerSet          m_children;
    WorkerFunc               m_worker;
    std::promise<bool>       m_promise_done;
    std::future<bool>        m_is_done;
    size_t                   m_parent_count;
    size_t                   m_parent_done_count;
    Work::Conditional        m_trigger_condition;
};

}

#endif /* TREE_OF_WORK */
