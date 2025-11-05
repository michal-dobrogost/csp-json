/// Generate two sets of variables X and Y with all variables in one set
/// sharing the same domain size. Random constraints between X-X, Y-Y and X-Y
/// variables are generated similar to URBCSP.

const std = @import("std");
const argsParser = @import("args");
const cj_gen_bipartite = @import("cj_gen_bipartite");
const c = @cImport({
    @cInclude("cj/cj-csp.h");
    @cInclude("cj/cj-csp-io.h");
    @cDefine("_NO_CRT_STDIO_INLINE", "1");
    @cInclude("stdio.h");
});

const assert = std.debug.assert;
const ArrayListM = std.array_list.Managed;

var gpa = std.heap.GeneralPurposeAllocator(.{}){};
const alloc = gpa.allocator();
const malloc = std.heap.c_allocator;

const Args = struct {
    num_vars1: u64 = 0,
    num_vars2: u64 = 0,
    domain_size1: u64 = 0,
    domain_size2: u64 = 0,
    num_constraints: u64 = 0,
    num_constraints1: u64 = 0,
    num_constraints2: u64 = 0,
    num_constraint_types: u64 = 0,
    num_constraint_types1: u64 = 0,
    num_constraint_types2: u64 = 0,
    num_no_goods: u64 = 0,
    num_no_goods1: u64 = 0,
    num_no_goods2: u64 = 0,
    seed: u64 = 0,
    pub const shorthands = .{
        .n = "num_vars1",
        .N = "num_vars2",
        .d = "domain_size1",
        .D = "domain_size2",
        .c = "num_constraints1",
        .C = "num_constraints2",
        .k = "num_constraint_types1",
        .K = "num_constraint_types2",
        .t = "num_no_goods1",
        .T = "num_no_goods2",
        .s = "seed"
    };
};

pub fn main() !void {
    const argsParseResult = argsParser.parseForCurrentProcess(Args, alloc, .print) catch return;
    defer argsParseResult.deinit();
    const args = argsParseResult.options;
    try checkArgs(args);

    var prng = std.Random.DefaultPrng.init(args.seed);
    const rand = prng.random();

    var csp: c.CjCsp = c.cjCspInit();
    defer c.cjCspFree(&csp);

    // Meta
    csp.meta.id = try std.fmt.allocPrintSentinel(malloc, "cj-gen-bipartite/n{}N{}d{}D{}cC{}c{}C{}kK{}k{}K{}tT{}t{}T{}s{}", args, 0);
    csp.meta.algo = try std.fmt.allocPrintSentinel(malloc, "cj-gen-bipartite", .{}, 0);

    var writerAlloc = std.Io.Writer.Allocating.init(malloc);
    try std.json.fmt(args, .{}).format(&writerAlloc.writer);
    csp.meta.paramsJSON = try writerAlloc.toOwnedSliceSentinel(0);

    // Domains
    csp.domainsSize = 2;
    csp.domains = c.cjDomainArray(csp.domainsSize);
    if (csp.domains == null) { return; }
    defer c.cjDomainArrayFree(&csp.domains, csp.domainsSize);
    try genDomain(args.domain_size1, @ptrCast(&csp.domains[0]));
    try genDomain(args.domain_size2, @ptrCast(&csp.domains[1]));

    // Variables
    csp.vars = c.cjIntTuplesInit();
    var cjErr = c.cjIntTuplesAlloc(@intCast(args.num_vars1 + args.num_vars2), -1, &csp.vars);
    for (0..args.num_vars1) |i| { csp.vars.data[i] = 0; }
    for (0..args.num_vars2) |i| { csp.vars.data[args.num_vars1 + i] = 1; }

    // Constraint Definitions
    csp.constraintDefsSize = @intCast(args.num_constraint_types1 + args.num_constraint_types2 + args.num_constraint_types);
    csp.constraintDefs = c.cjConstraintDefArray(csp.constraintDefsSize);
    if (csp.constraintDefs == null) { return error.CjError; }
    for (0..args.num_constraint_types1) |i| {
        try genNoGoods(rand, args.domain_size1, args.domain_size1, args.num_no_goods1, @ptrCast(&csp.constraintDefs[i]));
    }
    for (0..args.num_constraint_types2) |i| {
        try genNoGoods(rand, args.domain_size2, args.domain_size2, args.num_no_goods2, @ptrCast(&csp.constraintDefs[args.num_constraint_types1 + i]));
    }
    for (0..args.num_constraint_types) |i| {
        try genNoGoods(rand, args.domain_size1, args.domain_size2, args.num_no_goods, @ptrCast(&csp.constraintDefs[args.num_constraint_types1 + args.num_constraint_types2 + i]));
    }

    // Constraints
    csp.constraintsSize = @intCast(args.num_constraints1 + args.num_constraints2 + args.num_constraints);
    csp.constraints = c.cjConstraintArray(csp.constraintsSize);
    if (csp.constraints == null) { return error.CjError; }
    try genConstraints(rand, 0, args.num_vars1, 0, args.num_vars1, csp.constraints[0..args.num_constraints1], 0, args.num_constraint_types1);
    const num_constraints12 = args.num_constraints1 + args.num_constraints2;
    const num_constraint_types12 = args.num_constraint_types1 + args.num_constraint_types2;
    try genConstraints(rand, args.num_vars1, @intCast(csp.vars.size), args.num_vars1, @intCast(csp.vars.size), csp.constraints[args.num_constraints1..num_constraints12], args.num_constraint_types1, num_constraint_types12);
    try genConstraints(rand, 0, args.num_vars1, args.num_vars1, @intCast(csp.vars.size), csp.constraints[num_constraints12..@intCast(csp.constraintsSize)], num_constraint_types12, @intCast(csp.constraintDefsSize));

    //cjErr = c.cjCspNormalize(&csp);
    cjErr = c.cjCspJsonPrint(c.stdout(), &csp);
}

fn Tuple2(comptime T: type) type {
    return struct {
        x: T,
        y: T,
    };
}

fn genDomain(size:usize, domain: *c.CjDomain) !void {
    domain.*.type = c.CJ_DOMAIN_VALUES;
    const cjErr = c.cjIntTuplesAlloc(@intCast(size), -1, &domain.*.unnamed_0.values);
    if (cjErr != c.CJ_ERROR_OK) { return error.CjError; }
    for (0..size) |i| {
        domain.unnamed_0.values.data[i] = @intCast(i);
    }
}

fn genNoGoods(rand: std.Random, domSizeX:usize, domSizeY:usize, nTuples:usize, constraintDef:*c.CjConstraintDef) !void {
    constraintDef.type = c.CJ_CONSTRAINT_DEF_NO_GOODS;
    const arity = 2;
    const cjErr = c.cjIntTuplesAlloc(@intCast(nTuples), arity, &constraintDef.unnamed_0.noGoods);
    if (cjErr != c.CJ_ERROR_OK) { return error.CjError; }

    const ValTuple = Tuple2(usize);
    const valTuples = try alloc.alloc(ValTuple, domSizeX * domSizeY);
    defer alloc.free(valTuples);
    for (0..domSizeX) |x| {
        for (0..domSizeY) |y| {
            valTuples[x * domSizeY + y] = ValTuple{.x = x, .y = y};
        }
    }
    rand.shuffle(ValTuple, valTuples);

    for (0..nTuples) |i| {
        constraintDef.unnamed_0.noGoods.data[2*i + 0] = @intCast(valTuples[i].x);
        constraintDef.unnamed_0.noGoods.data[2*i + 1] = @intCast(valTuples[i].y);
    }
}

fn genConstraints(rand: std.Random, startX:u64, endX:u64, startY:u64, endY:u64, constraints:[]c.CjConstraint, startDefs:u64, endDefs:u64) !void {
    const VarTuple = Tuple2(usize);
    const varTuples = try alloc.alloc(VarTuple, (endX - startX) * (endY - startY));
    defer alloc.free(varTuples);
    var iTuple:u64 = 0;
    for (startX..endX) |x| {
        for (startY..endY) |y| {
            varTuples[iTuple] = VarTuple{.x = x, .y = y};
            iTuple += 1;
        }
    }
    rand.shuffle(VarTuple, varTuples);

    for (0..constraints.len) |i| {
        constraints[i].id = @intCast(startDefs + i % (endDefs - startDefs));
        const numVars = 2;
        const arity = -1;
        const cjErr = c.cjIntTuplesAlloc(numVars, arity, &constraints[i].vars);
        if (cjErr != c.CJ_ERROR_OK) { return error.CjError; }
        constraints[i].vars.data[0] = @intCast(varTuples[i].x);
        constraints[i].vars.data[1] = @intCast(varTuples[i].y);
    }
}

fn checkArgs(a: Args) !void {
    if (a.num_vars1 == 0) {
        return error.num_vars1_not_specified;
    }
    if (a.num_vars2 == 0) {
        return error.num_vars2_not_specified;
    }
    if (a.domain_size1 == 0) {
        return error.domain_size1_not_specified;
    }
    if (a.domain_size2 == 0) {
        return error.domain_size2_not_specified;
    }
    if (a.num_constraints == 0) {
        return error.num_constraints_not_specified;
    }
    if (a.num_constraints1 == 0) {
        return error.num_constraints1_not_specified;
    }
    if (a.num_constraints2 == 0) {
        return error.num_constraints2_not_specified;
    }
    if (a.num_constraint_types == 0) {
        return error.num_constraint_types_not_specified;
    }
    if (a.num_constraint_types1 == 0) {
        return error.num_constraint_types1_not_specified;
    }
    if (a.num_constraint_types2 == 0) {
        return error.num_constraint_types2_not_specified;
    }
    if (a.num_no_goods == 0) {
        return error.num_no_goods_not_specified;
    }
    if (a.num_no_goods1 == 0) {
        return error.num_no_goods1_not_specified;
    }
    if (a.num_no_goods2 == 0) {
        return error.num_no_goods2_not_specified;
    }
    if (a.num_constraints1 > (a.num_vars1 * a.num_vars1)) {
        return error.num_constraints1_too_large;
    }
    if (a.num_constraints2 > (a.num_vars2 * a.num_vars2)) {
        return error.num_constraints2_too_large;
    }
    if (a.num_constraints > (a.num_vars1 * a.num_vars2)) {
        return error.num_constraints_too_large;
    }
    if (a.num_constraint_types1 > a.num_constraints1) {
        return error.num_constraint_types1_too_large;
    }
    if (a.num_constraint_types2 > a.num_constraints2) {
        return error.num_constraint_types2_too_large;
    }
    if (a.num_constraint_types > a.num_constraints) {
        return error.num_constraint_types_too_large;
    }
    if (a.num_no_goods1 > (a.domain_size1 * a.domain_size1)) {
        return error.num_no_goods1_too_large;
    }
    if (a.num_no_goods2 > (a.domain_size2 * a.domain_size2)) {
        return error.num_no_goods2_too_large;
    }
    if (a.num_no_goods > (a.domain_size1 * a.domain_size2)) {
        return error.num_no_goods_too_large;
    }
}
