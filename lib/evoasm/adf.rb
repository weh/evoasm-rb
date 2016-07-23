require 'evoasm/search'

module Evoasm
  class ADF < FFI::AutoPointer

    def initialize(other_ptr)
      ptr = Libevoasm.adf_alloc
      unless Libevoasm.adf_clone other_ptr, ptr
        Libevoasm.adf_free(ptr)
        raise Error.last
      end
      super ptr
    end

    def self.release(ptr)
      Libevoasm.adf_destroy(ptr)
      Libevoasm.adf_free(ptr)
    end

    def run(*input_example)
      run_all(input_example).first
    end

    def run_all(*input_examples)
      input = Libevoasm::ADFInput.new(input_examples)
      output = Libevoasm::ADFOutput.new

      unless Libevoasm.adf_run self, input, output
        raise Libevoasm::Error.last
      end
      output_ary = output.to_a

      Libevoasm.adf_io_destroy output

      output_ary
    end

    def size
      Libevoasm.adf_size self
    end

    def clone
      self.class.new self
    end

    def eliminate_introns!
      unless Libevoasm.adf_eliminate_introns self
        raise Libevoasm::Error.last
      end
    end

    def eliminate_introns
      clone.tap do |adf|
        adf.eliminate_introns!
      end
    end

    def to_gv
      require 'gv'

      graph = GV::Graph.open 'g'

      disasms = []
      addrs = []

      size = self.size
      size.times do |kernel_index|
        code_len_ptr = FFI::MemoryPointer.new :size_t
        code_ptr = Libevoasm.adf_code self, kernel_index, code_len_ptr

        code_len = code_len_ptr.read_size_t
        code = code_ptr.read_string(code_len)

        disasm = X64.disassemble code, code_ptr.address
        disasms[kernel_index] = disasm
        addrs[kernel_index] = disasm.first.first
      end

      size.times do |kernel_index|
        label = '<TABLE BORDER="0" CELLBORDER="1" CELLSPACING="0">'

        label << '<TR>'
        label << %Q{<TD COLSPAN="3"><B>Kernel #{kernel_index}</B></TD>}
        label << '</TR>'

        disasm = disasms[kernel_index]
        addr = addrs[kernel_index]
        jmp_addrs = []

        disasm.each do |line|
          op_str = line[2]

          label << '<TR>'
          label << %Q{<TD ALIGN="LEFT">0x#{line[0].to_s 16}</TD>}
          label << %Q{<TD ALIGN="LEFT">#{line[1]}</TD>}

          if op_str =~ /0x(\h+)/
            jmp_addr = Integer($1, 16)
            jmp_addrs << jmp_addr
            port = jmp_addr
          else
            port = ''
          end
          label << %Q{<TD ALIGN="LEFT" PORT="#{port}">#{op_str}</TD>}
          label << '</TR>'
        end
        label << '</TABLE>'

        node = graph.node addr.to_s,
                          shape: :none,
                          label: graph.html(label)

        succs = [kernel_index + 1, Libevoasm.adf_kernel_alt_succ(self, kernel_index)].select do |succ|
          succ < size - 1
        end

        succs.each do |successor|
          succ_addr = addrs[successor.index]
          tail_port =
            if jmp_addrs.include? succ_addr
              # Remove, in case we the same
              # successor multiple times
              # only one of which goes through the jump
              jmp_addrs.delete succ_addr
              succ_addr.to_s
            else
              's'
            end
          graph.edge 'e', node, graph.node(succ_addr.to_s), tailport: tail_port, headport: 'n'
        end
      end

      graph
    end
  end
end
