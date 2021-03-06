/*
 *  Multi2Sim
 *  Copyright (C) 2012  Rafael Ubal (ubal@ece.neu.edu)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <cassert>
#include <fstream>
#include <iomanip>
#include <sstream>

#include <lib/cpp/Misc.h>

#include "Binary.h"
#include "Disassembler.h"
#include "Instruction.h"

namespace SI {

std::unique_ptr<Disassembler> Disassembler::instance;
std::string Disassembler::binary_file;

void Disassembler::RegisterOptions() {
  // Get command line object
  misc::CommandLine* command_line = misc::CommandLine::getInstance();

  // Category
  command_line->setCategory("Southern Islands");

  // Option --si-disasm <file>
  command_line->RegisterString(
      "--si-disasm <file>", binary_file,
      "Disassemble the Southern Islands ELF file provided in "
      "<arg>, using the internal Southern Islands "
      "disassembler. This option is incompatible with any "
      "other option.");

  // Incompatible options
  command_line->setIncompatible("--si-disasm");
}

void Disassembler::ProcessOptions() {
  // Run SI disassembler
  if (!binary_file.empty()) {
    // Get disassembler singleton
    Disassembler* disassembler = Disassembler::getInstance();

    // Disassemble binary
    disassembler->DisassembleBinary(binary_file);
    exit(0);
  }
}

Disassembler::Disassembler() : comm::Disassembler("SouthernIslands") {
  Instruction::Info* info;
  int i;

  // Type size assertions
  assert(sizeof(Instruction::Register) == 4);

// Read information about all instructions
#define DEFINST(_name, _fmt_str, _fmt, _op, _size, _flags) \
  info = &inst_info[Instruction::Opcode_##_name];          \
  info->opcode = Instruction::Opcode_##_name;              \
  info->name = #_name;                                     \
  info->fmt_str = _fmt_str;                                \
  info->fmt = Instruction::Format##_fmt;                   \
  info->op = _op;                                          \
  info->size = _size;                                      \
  info->flags = (Instruction::Flag)_flags;
#include "Instruction.def"
#undef DEFINST

  // Tables of pointers to 'inst_info'
  for (i = 1; i < Instruction::OpcodeCount; i++) {
    info = &inst_info[i];

    if (info->fmt == Instruction::FormatSOPP) {
      assert(misc::inRange(info->op, 0, dec_table_sopp_count - 1));
      dec_table_sopp[info->op] = info;
      continue;
    } else if (info->fmt == Instruction::FormatSOPC) {
      assert(misc::inRange(info->op, 0, dec_table_sopc_count - 1));
      dec_table_sopc[info->op] = info;
      continue;
    } else if (info->fmt == Instruction::FormatSOP1) {
      assert(misc::inRange(info->op, 0, dec_table_sop1_count - 1));
      dec_table_sop1[info->op] = info;
      continue;
    } else if (info->fmt == Instruction::FormatSOPK) {
      assert(misc::inRange(info->op, 0, dec_table_sopk_count - 1));
      dec_table_sopk[info->op] = info;
      continue;
    } else if (info->fmt == Instruction::FormatSOP2) {
      assert(misc::inRange(info->op, 0, dec_table_sop2_count - 1));
      dec_table_sop2[info->op] = info;
      continue;
    } else if (info->fmt == Instruction::FormatSMRD) {
      assert(misc::inRange(info->op, 0, dec_table_smrd_count - 1));
      dec_table_smrd[info->op] = info;
      continue;
    } else if (info->fmt == Instruction::FormatVOP3a ||
               info->fmt == Instruction::FormatVOP3b) {
      int i;

      assert(misc::inRange(info->op, 0, dec_table_vop3_count - 1));
      dec_table_vop3[info->op] = info;
      if (info->flags & Instruction::FlagOp8) {
        for (i = 1; i < 8; i++) {
          dec_table_vop3[info->op + i] = info;
        }
      }
      if (info->flags & Instruction::FlagOp16) {
        for (i = 1; i < 16; i++) {
          dec_table_vop3[info->op + i] = info;
        }
      }
      continue;
    } else if (info->fmt == Instruction::FormatVOPC) {
      assert(misc::inRange(info->op, 0, dec_table_vopc_count - 1));
      dec_table_vopc[info->op] = info;
      continue;
    } else if (info->fmt == Instruction::FormatVOP1) {
      assert(misc::inRange(info->op, 0, dec_table_vop1_count - 1));
      dec_table_vop1[info->op] = info;
      continue;
    } else if (info->fmt == Instruction::FormatVOP2) {
      assert(misc::inRange(info->op, 0, dec_table_vop2_count - 1));
      dec_table_vop2[info->op] = info;
      continue;
    } else if (info->fmt == Instruction::FormatVINTRP) {
      assert(misc::inRange(info->op, 0, dec_table_vintrp_count - 1));
      dec_table_vintrp[info->op] = info;
      continue;
    } else if (info->fmt == Instruction::FormatDS) {
      assert(misc::inRange(info->op, 0, dec_table_ds_count - 1));
      dec_table_ds[info->op] = info;
      continue;
    } else if (info->fmt == Instruction::FormatMTBUF) {
      assert(misc::inRange(info->op, 0, dec_table_mtbuf_count - 1));
      dec_table_mtbuf[info->op] = info;
      continue;
    } else if (info->fmt == Instruction::FormatMUBUF) {
      assert(misc::inRange(info->op, 0, dec_table_mubuf_count - 1));
      dec_table_mubuf[info->op] = info;
      continue;
    } else if (info->fmt == Instruction::FormatMIMG) {
      assert(misc::inRange(info->op, 0, dec_table_mimg_count - 1));
      dec_table_mimg[info->op] = info;
      continue;
    } else if (info->fmt == Instruction::FormatEXP) {
      assert(misc::inRange(info->op, 0, dec_table_exp_count - 1));
      dec_table_exp[info->op] = info;
      continue;
    } else {
      std::cerr << "warning: '" << info->name << "' not indexed\n";
    }
  }
}

Disassembler* Disassembler::getInstance() {
  // Instance already exists
  if (instance.get()) return instance.get();

  // Create instance
  instance.reset(new Disassembler());
  return instance.get();
}

void Disassembler::parseGPRs(ELFReader::Section* section, int* vgpr,
                             int* sgpr) {
  const char* buffer = section->getBuffer();
  int size = section->getSize();

  const char* original_buffer = buffer;
  int rel_addr = 0;

  Instruction::Format format;
  Instruction::Bytes* bytes;

  Instruction inst;

  // Reset counter, it will be updated in decode
  num_vgpr = -1;
  num_sgpr = -1;

  std::stringstream ss;

  // Read through instructions to find labels.
  while (buffer < original_buffer + size) {
    // Decode instruction
    inst.Decode(buffer, rel_addr);
    format = inst.getFormat();
    bytes = inst.getBytes();

    // Get GPR info
    ss.str("");
    inst.Dump(ss);

    // If ENDPGM, break.
    if (format == Instruction::FormatSOPP && bytes->sopp.op == 1) break;

    buffer += inst.getSize();
    rel_addr += inst.getSize();
  }

  *vgpr = num_vgpr + 1;
  *sgpr = num_sgpr + 1;

  // Reset counter
  num_vgpr = -1;
  num_sgpr = -1;
}

void Disassembler::DisassembleBuffer(std::ostream& os, const char* buffer,
                                     int size) {
  std::stringstream ss;

  const char* original_buffer = buffer;

  int inst_count = 0;
  int rel_addr = 0;

  int label_addr[size / 4];  // A list of created labels sorted by rel_addr.

  int* next_label = &label_addr[0];  // The next label to dump.
  int* end_label = &label_addr[0];   // The address after the last label.

  Instruction::Format format;
  Instruction::Bytes* bytes;

  Instruction inst;

  // Read through instructions to find labels.
  while (buffer < original_buffer + size) {
    // Decode instruction
    inst.Decode(buffer, rel_addr);
    format = inst.getFormat();
    bytes = inst.getBytes();

    // If ENDPGM, break.
    if (format == Instruction::FormatSOPP && bytes->sopp.op == 1) break;

    /* If the instruction branches, insert the label into
     * the sorted list. */
    if (format == Instruction::FormatSOPP &&
        (bytes->sopp.op >= 2 && bytes->sopp.op <= 9)) {
      short simm16 = bytes->sopp.simm16;
      int se_simm = simm16;
      int label = rel_addr + (se_simm * 4) + 4;

      // Find position to insert label.
      int* t_label = &label_addr[0];

      while (t_label < end_label && *t_label < label) t_label++;

      if (label != *t_label || t_label == end_label) {
        // Shift labels after position down.
        int* t2_label = end_label;

        while (t2_label > t_label) {
          *t2_label = *(t2_label - 1);
          t2_label--;
        }
        end_label++;

        // Insert the new label.
        *t_label = label;
      }
    }

    buffer += inst.getSize();
    rel_addr += inst.getSize();
  }

  // Reset to disassemble.
  buffer = original_buffer;
  rel_addr = 0;

  // Disassemble
  while (buffer < original_buffer + size) {
    // Parse the instruction
    inst.Decode(buffer, rel_addr);
    format = inst.getFormat();
    bytes = inst.getBytes();
    inst_count++;

    if (getenv("M2CDISASM")) {
      // Dump a label if necessary.
      if (*next_label == rel_addr && next_label != end_label) {
        os << misc::fmt("\tlabel_%04X:\n", rel_addr / 4);
        next_label++;
      }
      // Dump the instruction
      ss.str("");
      ss << '\t';
      inst.Dump(ss);

      // Spaces
      if (ss.str().length() < 59)
        ss << std::string(59 - ss.str().length(), ' ');

      // Hex dump
      os << ss.str();
      os << misc::fmt(" // %08X: %08X", rel_addr, bytes->word[0]);
      if (inst.getSize() == 8) os << misc::fmt(" %08X", bytes->word[1]);
      os << '\n';

    } else {
      // Dump a label if necessary.
      if (*next_label == rel_addr && next_label != end_label) {
        os << misc::fmt("label_%04X:\n", rel_addr / 4);
        next_label++;
      }

      // Dump the instruction
      ss.str("");
      ss << ' ';
      inst.Dump(ss);

      // Spaces
      if (ss.str().length() < 59)
        ss << std::string(59 - ss.str().length(), ' ');

      // Hex dump
      os << ss.str();
      os << misc::fmt(" // %08X: %08X", rel_addr, bytes->word[0]);
      if (inst.getSize() == 8) os << misc::fmt(" %08X", bytes->word[1]);
      os << '\n';
    }

    // Break at end of program.
    if (format == Instruction::FormatSOPP && bytes->sopp.op == 1) break;

    // Increment instruction pointer
    buffer += inst.getSize();
    rel_addr += inst.getSize();
  }
}

void Disassembler::DisassembleBinary(const std::string& path) {
  // Load ELF file
  ELFReader::File file(path);

  std::unordered_map<std::string, std::string> kernel_arguments;

  for (int i = 0; i < file.getNumSymbols(); i++) {
    // Get symbol
    ELFReader::Symbol* symbol = file.getSymbol(i);
    std::string symbol_name = symbol->getName();

    // Get arguments from metadata
    if (getenv("M2CDISASM")) {
      if (misc::StringPrefix(symbol_name, "__OpenCL_") &&
          misc::StringSuffix(symbol_name, "_metadata")) {
        // Init entry in hashtable
        std::string kernel_name =
            symbol_name.substr(9, symbol_name.length() - 18);
        std::string arguments = "";

        ELFReader::Symbol* metadata_symbol = symbol;
        std::istringstream metadata_stream;
        metadata_symbol->getStream(metadata_stream);

        std::string line;
        std::vector<std::string> token_list;

        for (;;) {
          // Read the next line
          std::getline(metadata_stream, line);
          misc::StringTokenize(line, token_list, ";:");

          // Stop when ARGEND is found or line is empty
          if (!token_list.size() || token_list.front() == "ARGEND") {
            token_list.clear();
            break;
          }

          // Value
          if (token_list.front() == "value") {
            // Token 1 - Name
            token_list.erase(token_list.begin());
            std::string name = *token_list.begin();

            // Token 2 - Data type
            token_list.erase(token_list.begin());
            std::string data_type = *token_list.begin();

            // Token 3 - Number of elements
            token_list.erase(token_list.begin());
            std::string num_elems = *token_list.begin();

            // Token 4 - Constant buffer
            token_list.erase(token_list.begin());
            std::string constant_buffer_num = *token_list.begin();

            // Token 5 - Constant offset
            token_list.erase(token_list.begin());
            std::string constant_offset = *token_list.begin();

            // Add argument and clear token list
            arguments += "\t";
            arguments += data_type + " " + name + " " + constant_offset + "\n";
            token_list.clear();
            continue;
          }

          // Pointer
          if (token_list.front() == "pointer") {
            // Token 1 - Name
            token_list.erase(token_list.begin());
            std::string name = *token_list.begin();

            // Token 2 - Data type
            token_list.erase(token_list.begin());
            std::string data_type = *token_list.begin();

            // Token 3 - Number of elements
            token_list.erase(token_list.begin());
            std::string num_elems = *token_list.begin();

            // Token 4 - Constant buffer
            token_list.erase(token_list.begin());
            std::string constant_buffer_num = *token_list.begin();

            // Token 5 - Constant offset
            token_list.erase(token_list.begin());
            std::string constant_offset = *token_list.begin();

            // Token 6 - Memory scope
            token_list.erase(token_list.begin());
            std::string arg_scope_string = *token_list.begin();

            // Token 7 - Buffer number
            token_list.erase(token_list.begin());
            std::string buffer_num = *token_list.begin();

            // Token 8 - Alignment
            token_list.erase(token_list.begin());
            std::string alignment = *token_list.begin();

            // Token 9 - Access type
            token_list.erase(token_list.begin());
            std::string arg_access_type_string = *token_list.begin();

            // Add argument and clear token list
            arguments += "\t";
            arguments += data_type;
            if (num_elems != "1") arguments += "[" + num_elems + "]";
            arguments += "* " + name + " " + constant_offset + " ";
            arguments += arg_scope_string + buffer_num + " " +
                         arg_access_type_string + "\n";
            token_list.clear();
            continue;
          }

          token_list.clear();
        }

        kernel_arguments[kernel_name] = arguments;
      }
    }
  }

  // Decode external ELF
  for (int i = 0; i < file.getNumSymbols(); i++) {
    // Get symbol
    ELFReader::Symbol* symbol = file.getSymbol(i);
    std::string symbol_name = symbol->getName();

    /* If symbol is '__OpenCL_XXX_kernel', it points
     * to internal ELF */
    if (misc::StringPrefix(symbol_name, "__OpenCL_") &&
        misc::StringSuffix(symbol_name, "_kernel")) {
      // Symbol must point to valid content
      if (!symbol->getBuffer())
        throw Error(misc::fmt("%s: symbol '%s' without content", path.c_str(),
                              symbol_name.c_str()));

      // Output for M2C
      if (getenv("M2CDISASM")) {
        // Get kernel name
        std::string kernel_name =
            symbol_name.substr(9, symbol_name.length() - 16);
        std::cout << ".global " << kernel_name << "\n";
        std::cout << "\n";

        // Get the area of the text section pointed to
        // by the symbol
        std::istringstream symbol_stream;
        symbol->getStream(symbol_stream);

        // Copy the symbol data into a buffer
        auto buffer = misc::new_unique_array<char>(symbol->getSize());
        symbol_stream.read(buffer.get(), (unsigned)symbol->getSize());

        // Create internal ELF
        Binary binary(buffer.get(), symbol->getSize(), kernel_name);

        // Get section with Southern Islands ISA
        BinaryDictEntry* si_dict_entry = binary.GetSIDictEntry();
        ELFReader::Section* section = si_dict_entry->text_section;

        // Metadata
        std::cout << ".metadata\n\n";
        std::cout << "\tCOMPUTE_PGM_RSRC2:USER_SGPR = "
                  << si_dict_entry->compute_pgm_rsrc2->user_sgpr << "\n";
        std::cout << "\tCOMPUTE_PGM_RSRC2:TGID_X_EN = "
                  << si_dict_entry->compute_pgm_rsrc2->tgid_x_en << "\n";
        std::cout << "\tCOMPUTE_PGM_RSRC2:LDS_SIZE = "
                  << si_dict_entry->compute_pgm_rsrc2->lds_size << "\n";
        std::cout << "\n";

        for (unsigned i = 0; i < si_dict_entry->num_user_elements; ++i) {
          auto user_element = si_dict_entry->user_elements[i];
          std::cout << "\tuserElements[" << i
                    << "] = " + std::string(binary_user_data_map.MapValue(
                                    user_element.dataClass))
                    << ", " << user_element.apiSlot << ", "
                    << "s[" << user_element.startUserReg << ":"
                    << user_element.startUserReg + user_element.userRegCount - 1
                    << "]\n";
        }

        std::cout << "\n";
        std::cout << "\tFloatMode = 192\n";
        std::cout << "\tIeeeMode = 0\n";
        std::cout << "\trat_op = 0x0c00\n";
        std::cout << "\n";
        std::cout << "\t// VGPRs = " << si_dict_entry->num_vgpr << "\n";
        std::cout << "\t// SGPRs = " << si_dict_entry->num_sgpr << "\n";
        std::cout << "\n";
        std::cout << "\n";

        // Arguments
        std::cout << ".args\n";
        std::cout << kernel_arguments[kernel_name];

        std::cout << "\n";

        // Disassembly
        std::cout << ".text\n";
        DisassembleBuffer(std::cout, section->getBuffer(), section->getSize());
        std::cout << "\n\n\n";
      } else {
        // Get kernel name
        std::string kernel_name =
            symbol_name.substr(9, symbol_name.length() - 16);
        std::cout << "**\n** Disassembly for '__kernel " << kernel_name
                  << "'\n**\n\n";

        // Get the area of the text section pointed to
        // by the symbol
        std::istringstream symbol_stream;
        symbol->getStream(symbol_stream);

        // Copy the symbol data into a buffer
        auto buffer = misc::new_unique_array<char>(symbol->getSize());
        symbol_stream.read(buffer.get(), (unsigned)symbol->getSize());

        // Create internal ELF
        Binary binary(buffer.get(), symbol->getSize(), kernel_name);

        // Get section with Southern Islands ISA
        BinaryDictEntry* si_dict_entry = binary.GetSIDictEntry();
        ELFReader::Section* section = si_dict_entry->text_section;

        // Disassemble
        DisassembleBuffer(std::cout, section->getBuffer(), section->getSize());
        std::cout << "\n\n\n";
      }
    }
  }
}

}  // namespace SI
