COM_OBJS +=\
      com/ltype.o \
      com/comf.o \
      com/smempool.o \
      com/agraph.o \
      com/sgraph.o \
      com/rational.o \
      com/linsys.o \
      com/xmat.o \
      com/testbs.o \
      com/flty.o \
      com/bs.o

OPT_OBJS +=\
      opt/cfs_opt.o\
      opt/dbg.o\
      opt/goto_opt.o\
      opt/if_opt.o \
      opt/ir.o\
      opt/ir_bb.o\
      opt/ir_du.o\
      opt/du.o\
      opt/ir_cfg.o\
      opt/ir_simp.o\
      opt/ir_gvn.o\
      opt/ir_rce.o\
      opt/ir_dce.o\
      opt/ir_cp.o\
      opt/ir_lcse.o\
      opt/ir_gcse.o\
      opt/ir_licm.o\
      opt/ir_ivr.o\
      opt/ir_middle_opt.o\
      opt/ir_high_opt.o\
      opt/ir_expr_tab.o\
      opt/cdg.o\
      opt/ir_refine.o\
      opt/ir_rp.o\
      opt/ir_aa.o\
      opt/ir_ssa.o\
      opt/label.o\
      opt/data_type.o \
      opt/option.o\
      opt/region.o\
      opt/region_mgr.o\
      opt/util.o\
      opt/var.o\
      opt/md.o\
      opt/cfs_mgr.o\
      opt/pass_mgr.o\
      opt/inliner.o\
      opt/ipa.o\
      opt/callg.o\
      opt/loop.o\
      opt/ir_loop_cvt.o\
      opt/prdf.o

CFLAGS = -DFOR_DEX -D_DEBUG_ -O0 -g2 -Wno-write-strings -Wsign-promo \
        -Wsign-compare -Wpointer-arith -Wno-multichar -Winit-self \
        -Wstrict-aliasing=3 -finline-limit=10000000 -Wswitch #-Wall
        #-Werror=overloaded-virtual \

all: com_objs opt_objs
	ar rcs libxoc.a $(COM_OBJS) $(OPT_OBJS)
	@echo "success!!"

INC=-I opt -I com -I dex -I .
%.o:%.cpp
	@echo "build $<"
	gcc $(CFLAGS) $(INC) -c $< -o $@

com_objs: $(COM_OBJS)
opt_objs: $(OPT_OBJS)

clean:
	@find -name "*.o" | xargs rm -f
	@find -name "*.a" | xargs rm -f
	@find -name "*.dot" | xargs rm -f
	@find -name "*.exe" | xargs rm -f
	@find -name "*.elf" | xargs rm -f
	@find -name "*.out" | xargs rm -f
	@find -name "*.tmp" | xargs rm -f
	@find -name "*.vcg" | xargs rm -f
	@find -name "*.cxx" | xargs rm -f
	@find -name "*.asm" | xargs rm -f
	@find -name "*.swp" | xargs rm -f
	@find -name "*.swo" | xargs rm -f
	@find -name "*.LOGLOG" | xargs rm -f

