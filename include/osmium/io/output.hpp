#ifndef OSMIUM_IO_OUTPUT_HPP
#define OSMIUM_IO_OUTPUT_HPP

/*

This file is part of Osmium (http://osmcode.org/osmium).

Copyright 2013 Jochen Topf <jochen@topf.org> and others (see README).

Boost Software License - Version 1.0 - August 17th, 2003

Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.

*/

#include <functional>
#include <future>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <osmium/thread/debug.hpp>
#include <osmium/thread/queue.hpp>
#include <osmium/io/file.hpp>
#include <osmium/io/header.hpp>
#include <osmium/io/detail/read_write.hpp>
#include <osmium/memory/buffer.hpp>
#include <osmium/osm.hpp>
#include <osmium/handler.hpp>

namespace osmium {

    namespace io {

        typedef osmium::thread::Queue<std::future<std::string>> data_queue_type;

        class Output {

        protected:

            osmium::io::File m_file;
            data_queue_type& m_output_queue;

            Output(const Output&) = delete;
            Output(Output&&) = delete;

            Output& operator=(const Output&) = delete;
            Output& operator=(Output&&) = delete;

        public:

            Output(const osmium::io::File& file, data_queue_type& output_queue) :
                m_file(file),
                m_output_queue(output_queue) {
                m_file.open_for_output();
            }

            int fd() {
                return m_file.fd();
            }

            virtual ~Output() {
            }

            virtual void set_header(const osmium::io::Header&) {
            }

            virtual void handle_buffer(osmium::memory::Buffer&&) = 0;

            virtual void close() = 0;

        }; // class Output

        /**
         * This factory class is used to register file output formats and open
         * output files in these formats. You should not use this class directly.
         * Instead use the osmium::output::open() function. XXX
         */
        class OutputFactory {

        public:

            typedef std::function<osmium::io::Output*(const osmium::io::File&, data_queue_type&)> create_output_type;

        private:

            typedef std::map<osmium::io::Encoding*, create_output_type> encoding2create_type;

            encoding2create_type m_callbacks;

            OutputFactory() :
                m_callbacks() {
            }

        public:

            static OutputFactory& instance() {
                static OutputFactory factory;
                return factory;
            }

            bool register_output_format(std::vector<osmium::io::Encoding*> encodings, create_output_type create_function) {
                for (auto encoding : encodings) {
                    if (! m_callbacks.insert(encoding2create_type::value_type(encoding, create_function)).second) {
                        return false;
                    }
                }
                return true;
            }


            bool unregister_output_format(osmium::io::Encoding* encoding) {
                return m_callbacks.erase(encoding) == 1;
            }

            std::unique_ptr<osmium::io::Output> create_output(const osmium::io::File& file, data_queue_type& output_queue) {
                encoding2create_type::iterator it = m_callbacks.find(file.encoding());

                if (it != m_callbacks.end()) {
                    return std::unique_ptr<osmium::io::Output>((it->second)(file, output_queue));
                }

                throw osmium::io::File::FileEncodingNotSupported();
            }

        }; // class OutputFactory

        class FileOutput {

            data_queue_type& m_input_queue;
            int m_fd;

        public:

            FileOutput(data_queue_type& input_queue, int fd) :
                m_input_queue(input_queue),
                m_fd(fd) {
            }

            void operator()() {
                osmium::thread::set_thread_name("_osmium_output");
                std::future<std::string> data_future;
                std::string data;
                do {
                    m_input_queue.wait_and_pop(data_future);
                    data = data_future.get();
                    osmium::io::detail::reliable_write(m_fd, data.data(), data.size());
                } while (!data.empty());
            }

        }; // class FileOutput

        class Writer {

            osmium::io::File m_file;
            std::unique_ptr<Output> m_output;
            data_queue_type m_output_queue {};
            std::thread m_file_output;

            Writer(const Writer&) = delete;
            Writer& operator=(const Writer&) = delete;

        public:

            Writer(const osmium::io::File& file, const osmium::io::Header& header = osmium::io::Header()) :
                m_file(file),
                m_output(OutputFactory::instance().create_output(m_file, m_output_queue)) {
                m_output->set_header(header);
                FileOutput file_output(m_output_queue, m_output->fd());
                m_file_output = std::thread(file_output);
            }

            Writer(const std::string& filename = "", const osmium::io::Header& header = osmium::io::Header()) :
                m_file(filename),
                m_output(OutputFactory::instance().create_output(m_file, m_output_queue)) {
                m_output->set_header(header);
                FileOutput file_output(m_output_queue, m_output->fd());
                m_file_output = std::thread(file_output);
            }

            ~Writer() {
                close();
                if (m_file_output.joinable()) {
                    m_file_output.join();
                }
            }

            void operator()(osmium::memory::Buffer&& buffer) {
                m_output->handle_buffer(std::move(buffer));
            }

            void close() {
                m_output->close();
            }

        }; // class Writer

    } // namespace io

} // namespace osmium

#endif // OSMIUM_IO_OUTPUT_HPP
