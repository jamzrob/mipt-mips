 /* mips_instr.h - instruction parser for mips
 * @author Pavel Kryukov pavel.kryukov@phystech.edu
 * Copyright 2014-2017 MIPT-MIPS
 */

/** Edited by Ladin Oleg. */

#ifndef MIPS_INSTR_H
#define MIPS_INSTR_H

// Generic C++
#include <cassert>
#include <array>
#include <unordered_map>

// MIPT-MIPS modules
#include <infra/types.h>
#include <infra/wide_types.h>
#include <infra/macro.h>
#include <infra/string/cow_string.h>

#include "mips_register/mips_register.h"

inline int32 sign_extend(int16 v)  { return static_cast<int32>(v); }
inline int32 zero_extend(uint16 v) { return static_cast<int32>(v); }

template <typename T, size_t N>
inline T count_zeros(T value)
{
    T count = 0;
    for ( T i = N; i > 0; i >>= 1)
    {
        if ( ( value & i) != 0)
           break;
        count++;
    }
    return count;
}

template<size_t N, typename T>
T align_up(T value) { return ((value + ((1ull << N) - 1)) >> N) << N; }

template<typename T>
uint128 mips_multiplication(T x, T y) {
    return static_cast<uint128>(x) * static_cast<uint128>(y);
}

template<typename T>
uint128 mips_division(T x, T y) {
    //using T64 = doubled_t<T>;
    if (y == 0)
        return 0;
    //auto x1 = static_cast<T>(x);
    //auto y1 = static_cast<T>(y);

    using UT = doubled_t<T>; 
    //std::cout << static_cast<UT>(x) / static_cast<UT>(y) << "  " << static_cast<UT>(x) % static_cast<UT>(y) << std::endl;
    //const uint128 hi = static_cast<uint32>((static_cast<int32>(x) % static_cast<int32>(y))) & 0x000000000000000000000000ffffffff;
    //const uint64 lo = static_cast<uint32>((static_cast<int32>(x) / static_cast<int32>(y))) & 0x00000000ffffffff;
   
    const uint128 hi = ((static_cast<uint32>(static_cast<UT>(x) % static_cast<UT>(y))) & 0x000000000000000000000000ffffffff);
    const auto lo = static_cast<uint32>((static_cast<UT>(x) / static_cast<UT>(y))) & 0x00000000ffffffff;
    
    const uint64 hi1 = static_cast<uint32>((static_cast<UT>(x) % static_cast<UT>(y))) & 0x00000000ffffffff;

    std::cout << hi1 << "   " << lo << std::endl;
    
    return (hi << 64) | lo;

    /*
    const auto hi = static_cast<uint64>(x / y);
    const auto lo = static_cast<uint64>(x % y) & (((1) << 64) - 1);
    return (hi << 64) | lo;
    */
    /*
    if ( sizeof(T) == 4)
        return static_cast<uint128>(static_cast<uint32>(x1 / y1)) | (static_cast<uint128>(static_cast<uint32>(x1 % y1)) << 64);
    else
        return static_cast<uint128>(static_cast<uint64>(x / y)) | (static_cast<uint128>(static_cast<uint64>(x % y)) << 64);
    */
}

class MIPSInstr
{
    private:
        enum OperationType : uint8
        {
            OUT_R_ARITHM,
            OUT_R_ACCUM,
            OUT_R_DIVMULT,
            OUT_R_CONDM,
            OUT_R_SHIFT,
            OUT_R_SHAMT,
            OUT_R_JUMP,
            OUT_R_JUMP_LINK,
            OUT_R_SPECIAL,
            OUT_R_SUBTR,
            OUT_R_TRAP,
            OUT_R_MFLO,
            OUT_R_MTLO,
            OUT_R_MFHI,
            OUT_R_MTHI,
            OUT_I_ARITHM,
            OUT_I_BRANCH,
            OUT_I_BRANCH_0,
            OUT_RI_BRANCH_0,
            OUT_RI_TRAP,
            OUT_I_LOAD,
            OUT_I_LOADU,
            OUT_I_LOADR,
            OUT_I_LOADL,
            OUT_I_CONST,
            OUT_I_STORE,
            OUT_I_STOREL,
            OUT_I_STORER,
            OUT_J_JUMP,
            OUT_J_JUMP_LINK,
            OUT_RI_BRANCH_LINK,
            OUT_J_SPECIAL,
            OUT_SP2_COUNT,
            OUT_UNKNOWN
        } operation = OUT_UNKNOWN;

        enum class TrapType : uint8
        {
            NO_TRAP,
            EXPLICIT_TRAP,
        } trap = TrapType::NO_TRAP;

        const union _instr
        {
            const struct AsR
            {
                uint32 funct  :6;
                uint32 shamt  :5;
                uint32 rd     :5;
                uint32 rt     :5;
                uint32 rs     :5;
                uint32 opcode :6;
            } asR;
            const struct AsI
            {
                uint32 imm    :16;
                uint32 rt     :5;
                uint32 rs     :5;
                uint32 opcode :6;
            } asI;
            const struct AsJ
            {
                uint32 imm    :26;
                uint32 opcode :6;
            } asJ;

            const uint32 raw;

            _instr() : raw(NO_VAL32) { };
            explicit _instr(uint32 bytes) : raw( bytes) { }

            static_assert( sizeof( AsR) == sizeof( uint32));
            static_assert( sizeof( AsI) == sizeof( uint32));
            static_assert( sizeof( AsJ) == sizeof( uint32));
            static_assert( sizeof( uint32) == 4);
        } instr;

        using Execute = void (MIPSInstr::*)();
        using Predicate = bool (MIPSInstr::*)() const;

        struct ISAEntry
        {
            std::string_view name;
            OperationType operation;
            uint8 mem_size;
            MIPSInstr::Execute function;
            uint8 mips_version;
        };

        static const std::unordered_map <uint8, MIPSInstr::ISAEntry> isaMapR;
        static const std::unordered_map <uint8, MIPSInstr::ISAEntry> isaMapRI;
        static const std::unordered_map <uint8, MIPSInstr::ISAEntry> isaMapIJ;
        static const std::unordered_map <uint8, MIPSInstr::ISAEntry> isaMapMIPS32;

        MIPSRegister src1 = MIPSRegister::zero;
        MIPSRegister src2 = MIPSRegister::zero;
        MIPSRegister dst = MIPSRegister::zero;

        uint32 v_imm = NO_VAL32;
        uint64 v_src1 = NO_VAL64;
        uint64 v_src2 = NO_VAL64;
        uint128 v_dst = NO_VAL64;
        uint16 shamt = NO_VAL16;
        Addr mem_addr = NO_VAL32;
        uint32 mem_size = NO_VAL32;

        // convert this to bitset
        bool complete   = false;
        bool writes_dst = true;
        bool _is_jump_taken = false;      // actual result

        Addr new_PC = NO_VAL32;

        const Addr PC = NO_VAL32;

#if 0
        std::string disasm = {};
#else
        CowString disasm = {};
#endif

        void init( const ISAEntry& entry);

        // Predicate helpers - unary
        bool lez() const { return static_cast<int32>( v_src1) <= 0; }
        bool gez() const { return static_cast<int32>( v_src1) >= 0; }
        bool ltz() const { return static_cast<int32>( v_src1) < 0; }
        bool gtz() const { return static_cast<int32>( v_src1) > 0; }

        // Predicate helpers - binary
        bool eq()  const { return v_src1 == v_src2; }
        bool ne()  const { 
        //    std::cout << static_cast<int64>( v_src1) << "    " << static_cast<int64>( v_src2) << std::endl;
            return static_cast<int64>( v_src1) != static_cast<int64>( v_src2); }
        bool geu() const { return v_src1 >= v_src2; }
        bool ltu() const { return v_src1 <  v_src2; }
        bool ge()  const { return static_cast<int32>( v_src1) >= static_cast<int32>( v_src2); }
        bool lt()  const { return static_cast<int32>( v_src1) <  static_cast<int32>( v_src2); }

        // Predicate helpers - immediate
        bool eqi() const { return static_cast<int32>( v_src1) == sign_extend( v_imm); }
        bool nei() const { return static_cast<int32>( v_src1) != sign_extend( v_imm); }
        bool lti() const { return static_cast<int32>( v_src1) <  sign_extend( v_imm); }
        bool gei() const { return static_cast<int32>( v_src1) >= sign_extend( v_imm); }

        // Predicate helpers - immediate unsigned
        bool ltiu() const { return v_src1 <  static_cast<uint32>(sign_extend( v_imm)); }
        bool geiu() const { return v_src1 >= static_cast<uint32>(sign_extend( v_imm)); }

        template <typename T>
        void execute_add()   { v_dst = static_cast<T>( v_src1) + static_cast<T>( v_src2); }

        template <typename UT, typename T>
        void execute_sub()   { v_dst = static_cast<UT>( static_cast<T>( v_src1) - static_cast<T>( v_src2)); }
        template <typename T>
        void execute_addi()  { v_dst = static_cast<T>( v_src1) + static_cast<T>( sign_extend( v_imm)); }

        template <typename T>
        void execute_addu()  { v_dst = static_cast<T>( v_src1) + static_cast<T>( v_src2); }
        template <typename T>
        void execute_subu()  { v_dst = static_cast<T>( v_src1) - static_cast<T>( v_src2); }
        template <typename UT, typename T>
        void execute_addiu() { v_dst = static_cast<UT>(static_cast<T>(v_src1) + static_cast<T>( sign_extend(v_imm))); }

        void execute_mult()   { v_dst = mips_multiplication<int32>(v_src1, v_src2); }
        void execute_multu()  { v_dst = mips_multiplication<uint32>(v_src1, v_src2); }
        void execute_dmult()  { v_dst = mips_multiplication<int64>(v_src1, v_src2); }
        void execute_dmultu() { v_dst = mips_multiplication<uint64>(v_src1, v_src2); }
        void execute_div()    { v_dst = mips_division<int32>(v_src1, v_src2); }
        void execute_ddiv()   { v_dst = mips_division<int64>(v_src1, v_src2); }
        void execute_divu()   { v_dst = mips_division<uint32>(v_src1, v_src2); }
        void execute_ddivu()  { v_dst = mips_division<uint64>(v_src1, v_src2); }
        void execute_move()   { v_dst = v_src1; }
       
        template <typename T>    
        void execute_sll()   { v_dst = static_cast<T>( v_src1) << shamt; }
        void execute_dsll32() { v_dst = v_src1 << (shamt + 32); }
        void execute_srl()   { v_dst = v_src1 >> shamt; }
      
        template <typename T, typename UT>
        void execute_sra()   { v_dst = static_cast<UT>( static_cast<T>( v_src1) >> shamt); }
        void execute_dsra32() { v_dst = v_src1 >> (shamt + 32); }       
        void execute_sllv()  { v_dst = static_cast<uint32>( v_src1) << v_src2; }
        void execute_dsllv()  { v_dst = v_src1 << v_src2; }
        template <typename T>
        void execute_srlv()   { v_dst = static_cast<T>( v_src1) >> static_cast<T>( v_src2); }
        void execute_dsrl()   { v_dst = v_src1 >> shamt; }
        void execute_dsrl32() { v_dst = v_src1 >> (shamt + 32); }
        template <typename T, typename UT>
        void execute_srav()   { v_dst = static_cast<UT>( static_cast<T>( v_src1) >> static_cast<UT>( v_src2)); }
        void execute_lui()    { v_dst = static_cast<uint32>( sign_extend( v_imm) << 0x10); }

        void execute_and()   { v_dst = v_src1 & v_src2; }
        void execute_or()    { v_dst = v_src1 | v_src2; }
        void execute_xor()   { v_dst = v_src1 ^ v_src2; }
        void execute_nor()   { v_dst = ~static_cast<uint64>(v_src1 | v_src2); }

        void execute_andi()  { v_dst = v_src1 & zero_extend(v_imm); }
        void execute_ori()   { v_dst = v_src1 | zero_extend(v_imm); }
        void execute_xori()  { v_dst = v_src1 ^ zero_extend(v_imm); }

        void execute_movn()  { execute_move(); writes_dst = (v_src2 != 0);}
        void execute_movz()  { execute_move(); writes_dst = (v_src2 == 0);}

        // MIPStion-templated method is a little-known feature of C++, but useful here
        template<Predicate p>
        void execute_set() { v_dst = static_cast<uint32>((this->*p)()); }

        template<Predicate p>
        void execute_trap() { if ((this->*p)()) trap = TrapType::EXPLICIT_TRAP; }

        template<Predicate p>
        void execute_branch()
        {
            _is_jump_taken = (this->*p)();
            if ( _is_jump_taken)
                new_PC += sign_extend( v_imm) << 2;
        }
        void execute_clo()  { v_dst = count_zeros<uint32,         0x80000000>( ~v_src1); }
        void execute_clz()  { v_dst = count_zeros<uint32,         0x80000000>(  v_src1); }
        void execute_dclo() { v_dst = count_zeros<uint64, 0x8000000000000000>( ~v_src1); }
        void execute_dclz() { v_dst = count_zeros<uint64, 0x8000000000000000>(  v_src1); }

        void execute_jump( Addr target)
        {
            _is_jump_taken = true;
            new_PC = target;
        }

        void execute_j()  { execute_jump((PC & 0xf0000000) | (v_imm << 2)); }
        void execute_jr() { execute_jump(align_up<2>(v_src1)); }
        void execute_jal()  { v_dst = new_PC; execute_jump((PC & 0xf0000000) | (v_imm << 2)); }
        void execute_jalr() { v_dst = new_PC; execute_jump(align_up<2>(v_src1)); }

        template<Predicate p>
        void execute_branch_and_link()
        {
            _is_jump_taken = (this->*p)();
            if ( _is_jump_taken)
            {
                v_dst = new_PC;
                new_PC += sign_extend( v_imm) << 2;
            }
        }

        void execute_syscall(){ };
        void execute_break()  { };

        void execute_unknown();

        void calculate_load_addr()  { mem_addr = v_src1 + sign_extend(v_imm); }
        void calculate_store_addr() { mem_addr = v_src1 + sign_extend(v_imm); }

        Execute function = &MIPSInstr::execute_unknown;
    public:
        MIPSInstr() = delete;

        explicit
        MIPSInstr( uint32 bytes, Addr PC = 0);

        const std::string_view Dump() const { return static_cast<std::string_view>(disasm); }
        bool is_same( const MIPSInstr& rhs) const {
            return PC == rhs.PC && instr.raw == rhs.instr.raw;
        }

        MIPSRegister get_src_num( uint8 index) const { return ( index == 0) ? src1 : src2; }
        MIPSRegister get_dst_num()  const { return dst;  }

        /* Checks if instruction can change PC in unusual way. */
        bool is_jump() const { return operation == OUT_J_JUMP         ||
                                      operation == OUT_J_JUMP_LINK    ||
                                      operation == OUT_RI_BRANCH_LINK ||
                                      operation == OUT_R_JUMP         ||
                                      operation == OUT_R_JUMP_LINK    ||
                                      operation == OUT_I_BRANCH_0     ||
                                      operation == OUT_RI_BRANCH_0    ||
                                      operation == OUT_I_BRANCH;     }
        bool is_jump_taken() const { return  _is_jump_taken; }

        bool is_load()  const { return operation == OUT_I_LOAD  ||
                                       operation == OUT_I_LOADU ||
                                       operation == OUT_I_LOADR ||
                                       operation == OUT_I_LOADL; }
        int8 is_accumulating_instr() const
        {
            return (operation == OUT_R_ACCUM) ? 1 : (operation == OUT_R_SUBTR) ? -1 : 0;
        }
        bool is_store() const { return operation == OUT_I_STORE  ||
                                       operation == OUT_I_STORER ||
                                       operation == OUT_I_STOREL; }
        bool is_nop() const { return instr.raw == 0x0u; }
        bool is_halt() const { return is_jump() && new_PC == 0; }

        bool is_conditional_move() const { return operation == OUT_R_CONDM; }

        bool has_trap() const { return trap != TrapType::NO_TRAP; }

        bool get_writes_dst() const { return writes_dst; }

        bool is_bubble() const { return is_nop() && PC == 0; }

        void set_v_src( uint64 value, uint8 index)
        {
            if ( index == 0)
                v_src1 = value;
            else
                v_src2 = value;
        }

        uint64 get_v_dst() const { return v_dst; }

        Addr get_mem_addr() const { return mem_addr; }
        uint32 get_mem_size() const { return mem_size; }
        Addr get_new_PC() const { return new_PC; }
        Addr get_PC() const { return PC; }

        void set_v_dst(uint64 value); // for loads
        uint64 get_v_src2() const { return v_src2; } // for stores

        uint128 get_bypassing_data() const
        {
            return ( dst.is_mips_hi()) ? v_dst << 64 : v_dst; 
        }

        void execute();
        void check_trap();
};

static inline std::ostream& operator<<( std::ostream& out, const MIPSInstr& instr)
{
    return out << instr.Dump();
}

#endif //MIPS_INSTR_H
