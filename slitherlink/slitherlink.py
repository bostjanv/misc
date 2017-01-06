import sys
from datetime import datetime

class Slitherlink:
    def __init__(self, constraints = None):
        if constraints is not None:
            self.rows = len(constraints)
            self.cols = len(constraints[0])
            self.constraints = constraints
            self.h_edges = [[0 for _ in range(self.cols)] for _ in range(self.rows + 1)]
            self.v_edges = [[0 for _ in range(self.cols + 1)] for _ in range(self.rows)]
            self.r = 0
            self.c = 0

    def copy(self):
        o = Slitherlink()
        o.rows = self.rows
        o.cols = self.cols
        o.constraints = self.constraints
        o.h_edges = [x[:] for x in self.h_edges]
        o.v_edges = [x[:] for x in self.v_edges]
        o.r = self.r
        o.c = self.c
        return o

    def count_neighbours(self, r, c):
        count = 0
        if r > 0 and self.v_edges[r - 1][c] == 1:
            count += 1
        if r < self.rows and self.v_edges[r][c] == 1:
            count += 1
        if c > 0 and self.h_edges[r][c - 1] == 1:
            count += 1
        if c < self.cols and self.h_edges[r][c] == 1:
            count += 1

        return count

    def count_cell_edges(self, r, c):
        count = self.h_edges[r][c] + self.h_edges[r + 1][c] + \
                self.v_edges[r][c] + self.v_edges[r][c + 1]
        return count

    def check_constraint(self, r, c):
        count = self.h_edges[r][c] + self.h_edges[r + 1][c] + \
                self.v_edges[r][c] + self.v_edges[r][c + 1]
        if count < self.constraints[r][c]:
            return True
        else:
            return False

    def is_right_valid(self):
        if (self.c == self.cols or self.h_edges[self.r][self.c] == 1) or \
                (self.count_neighbours(self.r, self.c + 1) > 1):
                    return False
        elif (self.r == 0 or self.check_constraint(self.r - 1, self.c)) and \
                (self.r == self.rows or self.check_constraint(self.r, self.c)):
                    return True
        else:
            return False

    def is_left_valid(self):
        if (self.c == 0 or self.h_edges[self.r][self.c - 1] == 1) or \
                (self.count_neighbours(self.r, self.c - 1) > 1):
                    return False
        elif (self.r == 0 or self.check_constraint(self.r - 1, self.c - 1)) and \
                (self.r == self.rows or self.check_constraint(self.r, self.c - 1)):
                    return True
        else:
            return False

    def is_down_valid(self):
        if (self.r == self.cols or self.v_edges[self.r][self.c] == 1) or \
                (self.count_neighbours(self.r + 1, self.c) > 1):
                    return False
        elif (self.c == 0 or self.check_constraint(self.r, self.c - 1)) and \
                (self.c == self.cols or self.check_constraint(self.r, self.c)):
                    return True
        else:
            return False

    def is_up_valid(self):
        if (self.r == 0 or self.v_edges[self.r - 1][self.c] == 1) or \
                (self.count_neighbours(self.r - 1, self.c) > 1):
                    return False
        elif (self.c == 0 or self.check_constraint(self.r - 1, self.c - 1)) and \
                (self.c == self.cols or self.check_constraint(self.r - 1, self.c)):
                    return True
        else:
            return False

    def move_right(self):
        assert(self.c < self.cols)
        self.h_edges[self.r][self.c] = 1
        self.c += 1

    def move_left(self):
        assert(self.c > 0)
        self.c -= 1
        self.h_edges[self.r][self.c] = 1

    def move_down(self):
        assert(self.r < self.rows)
        self.v_edges[self.r][self.c] = 1
        self.r += 1

    def move_up(self):
        assert(self.r > 0)
        self.r -= 1
        self.v_edges[self.r][self.c] = 1

    def show(self):
        for i in range(self.rows+1):
            for j in range(self.cols):
                if self.h_edges[i][j] == 1:
                    sys.stdout.write('*---')
                else:
                    sys.stdout.write('*   ')
            sys.stdout.write('*\n')

            if i < self.rows:
                for j in range(self.cols + 1):
                    tmp = [' ']*4
                    if self.v_edges[i][j] == 1:
                        tmp[0] = '|'
                    if j < self.cols and self.constraints[i][j] != 255:
                        tmp[2] = str(self.constraints[i][j])
                    sys.stdout.write(''.join(tmp))
                sys.stdout.write('\n')

def set_initial_position(p):
    for i in range(len(p.constraints)):
        for j in range(len(p.constraints[:])):
            if p.constraints[i][j] == 3:
                p.r, p.c = i, j
                return

def is_connected(p):
    for i in range(p.rows + 1):
        for j in range(p.cols + 1):
            n = p.count_neighbours(i, j)
            if n != 0 and n != 2:
                assert(n == 1)
                return False
    return True

def is_solved(p):
    for i in range(p.rows):
        for j in range(p.cols):
            c = p.constraints[i][j]
            if c != 255 and p.count_cell_edges(i, j) != c:
                return False
    return True

def reset_position(p):
    for i in range(p.rows + 1):
        for j in range(p.cols + 1):
            n = p.count_neighbours(i, j)
            if n == 1:
                p.r, p.c = i, j

def solve(p, counter):
    n = p.count_neighbours(p.r, p.c)
    assert(n <= 2)
    if n == 2:
        # We made a connection with a previous segment
        if is_connected(p):
            if is_solved(p):
                counter[0] += 1
                print('[{}] {}-th solution'.format(str(datetime.now()), counter[0]))
                p.show()
                sys.stdout.write('\n')
        else:
            reset_position(p)
            solve(p, counter)

    else:
        valid = [
            p.is_left_valid(),
            p.is_right_valid(),
            p.is_up_valid(),
            p.is_down_valid()
        ]

        # assert(any(valid))
        '''if not any(valid):
            print('nowhere to move')
            p.show()
            sys.stdout.write('\n')'''

        if valid[0]:
            p1 = p.copy()
            p1.move_left()
            solve(p1, counter)
        if valid[1]:
            p1 = p.copy()
            p1.move_right()
            solve(p1, counter)
        if valid[2]:
            p1 = p.copy()
            p1.move_up()
            solve(p1, counter)
        if valid[3]:
            p1 = p.copy()
            p1.move_down()
            solve(p1, counter)

def parse_puzzle(puzzle):
    fields = puzzle.split()
    assert(len(fields) == 3)
    rows = int(fields[0])
    cols = int(fields[1])
    assert(len(fields[2]) == rows * cols)
    constraints = [[0]*cols for _ in range(rows)]
    for i in range(rows):
        for j in range(cols):
            c = fields[2][i*cols + j]
            if c == '.':
                constraints[i][j] = 255
            else:
                constraints[i][j] = int(c)
    return constraints

'''
puzzles = [
  '6 6 ....0.33..1...12....20...1..11.2....',
  '7 7 ....231213.2...1..213.3.3..23.0.1.3.32..3.3.222.3',
  '7 7 .33.3....0..122.23.21....2.....2...32.32..1..22.3',
]

for p in puzzles:
    print('[{}]'.format(str(datetime.now())))
    constraints = parse_puzzle(p)
    s = Slitherlink(constraints)
    s.show()
    print('')
    counter = [0]
    set_initial_position(s)
    solve(s, counter)
'''

print('[{}]'.format(str(datetime.now())))
constraints = parse_puzzle('7 7 ....231213.2...1..213.3.3..23.0.1.3.32..3.3.222.3')
s = Slitherlink(constraints)
s.v_edges[6][0] = 1
s.h_edges[7][0] = 1
s.v_edges[6][7] = 1
s.h_edges[7][6] = 1
s.v_edges[3][2] = 1
s.h_edges[4][1] = 1
s.v_edges[3][3] = 1
s.h_edges[4][3] = 1

s.h_edges[4][0] = 1
s.v_edges[4][0] = 1
s.h_edges[5][0] = 1
s.h_edges[5][1] = 1
s.v_edges[5][2] = 1
# s.h_edges[6][1] = 1
# s.h_edges[6][2] = 1

s.r = 7
s.c = 1
s.show()

counter = [0]
solve(s, counter)
print('[{}]'.format(str(datetime.now())))

'''
constraints = parse_puzzle('3 3 ......3.3')
s = Slitherlink(constraints)
s.v_edges[2][0] = 1
s.h_edges[3][0] = 1
s.v_edges[2][3] = 1
s.h_edges[3][2] = 1
s.r = 2
s.c = 0
s.show()

counter = [0]
solve(s, counter)
'''
