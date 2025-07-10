/* A Bison parser, made by GNU Bison 3.0.4.  */

/* Bison interface for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

#ifndef YY_YY_HOME_JAVIER_CLIONPROJECTS_IDSPROJECT_CMAKE_BUILD_DEBUG_EXTERNAL_LIBVMI_LIBVMI_GRAMMAR_H_INCLUDED
# define YY_YY_HOME_JAVIER_CLIONPROJECTS_IDSPROJECT_CMAKE_BUILD_DEBUG_EXTERNAL_LIBVMI_LIBVMI_GRAMMAR_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int yydebug;
#endif

/* Token type.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    NUM = 258,
    KPGD = 259,
    LINUX_TASKS = 260,
    LINUX_MM = 261,
    LINUX_PID = 262,
    LINUX_NAME = 263,
    LINUX_PGD = 264,
    LINUX_ADDR = 265,
    LINUX_INIT_TASK = 266,
    LINUX_KASLR = 267,
    WIN_NTOSKRNL = 268,
    WIN_NTOSKRNL_VA = 269,
    WIN_TASKS = 270,
    WIN_PDBASE = 271,
    WIN_PID = 272,
    WIN_PNAME = 273,
    WIN_KDVB = 274,
    WIN_KDBG = 275,
    WIN_KPCR = 276,
    WIN_SYSPROC = 277,
    FREEBSD_NAME = 278,
    FREEBSD_PID = 279,
    FREEBSD_VMSPACE = 280,
    FREEBSD_PMAP = 281,
    FREEBSD_PGD = 282,
    SYSMAPTOK = 283,
    REKALL_PROFILE = 284,
    VOLATILITY_PROFILE = 285,
    OSTYPETOK = 286,
    PAGEMODETOK = 287,
    AARCH64_TTBR0_VA_WIDTH = 288,
    AARCH64_TTBR0_GRANULE_SIZE = 289,
    AARCH64_TTBR1_VA_WIDTH = 290,
    AARCH64_TTBR1_GRANULE_SIZE = 291,
    WORD = 292,
    FILENAME = 293,
    QUOTE = 294,
    OBRACE = 295,
    EBRACE = 296,
    SEMICOLON = 297,
    EQUALS = 298
  };
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED

union YYSTYPE
{
#line 249 "/home/javier/CLionProjects/IDSProject/external/libvmi/libvmi/config/grammar.y" /* yacc.c:1909  */

    char *str;

#line 102 "/home/javier/CLionProjects/IDSProject/cmake-build-debug/external/libvmi/libvmi/grammar.h" /* yacc.c:1909  */
};

typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;

int yyparse (void);

#endif /* !YY_YY_HOME_JAVIER_CLIONPROJECTS_IDSPROJECT_CMAKE_BUILD_DEBUG_EXTERNAL_LIBVMI_LIBVMI_GRAMMAR_H_INCLUDED  */
