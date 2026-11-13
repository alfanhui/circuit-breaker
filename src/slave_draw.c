#include "slave_draw.h"

#ifdef JO_COMPILE_WITH_DUAL_CPU_SUPPORT

/*
 * The meshes handed over this frame, in order. jo runs the slave's jobs in the order they're
 * given, so the n-th job draws the n-th mesh: each job takes the next one.
 *
 * Sharing memory between the two CPUs: each SH-2 has its own cache, and neither sees the other's
 * writes in it. Both caches write through to RAM, and jo clears the slave's cache before each
 * job (jo's slave.c), so what the master wrote before handing a job over is what the slave reads.
 * The slave's count (taken) is only read back by the slave, and reset by the master once the
 * slave is done (slave_draw_end has waited, and jo has cleared the master's cache then)
 */
#define SLAVE_MESHES_MAX 8 // a frame's: the floor and the boxes now, room for more

static const jo_3d_mesh *handed[SLAVE_MESHES_MAX];
static volatile int handed_count = 0; // written by the master
static volatile int taken = 0;		  // written by the slave
static bool begun = false;
// The jobs handed over this pass (slave_draw_job), in order, as the meshes: the n-th run_job runs
// the n-th
static slave_draw_job_fn jobs_given[SLAVE_JOBS_MAX]; // written by the master before each is queued
static volatile int jobs_count = 0;					 // written by the master
static volatile int jobs_taken = 0;					 // written by the slave
static volatile bool in_job = false;				 // set by the slave, running a job (only it reads it)

// A job on the slave: draw the next mesh handed over
static void draw_next(void)
{
	const int i = taken;

	if (i < handed_count)
		jo_3d_slave_mesh_draw(handed[i]);
	taken = i + 1;
}

void slave_draw_begin(void)
{
	handed_count = 0;
	taken = 0;
	jobs_count = 0;
	jobs_taken = 0;
	jo_3d_slave_begin(); // the slave starts from the matrix current now: the world's
	begun = true;
}

void slave_draw_mesh(const jo_3d_mesh *mesh)
{
	if (!begun || handed_count >= SLAVE_MESHES_MAX)
	{
		jo_3d_mesh_draw((jo_3d_mesh *)mesh); // no room (or no frame begun): the master draws it
		return;
	}
	handed[handed_count] = mesh;
	handed_count = handed_count + 1;
	jo_core_exec_on_slave(draw_next); // queued: the master doesn't wait
}

static void run_job(void)
{
	const int i = jobs_taken;

	in_job = true;
	if (i < jobs_count)
		jobs_given[i]();
	in_job = false;
	jobs_taken = i + 1;
}

void slave_draw_job(slave_draw_job_fn job)
{
	if (!begun || jobs_count >= SLAVE_JOBS_MAX)
	{
		job(); // no pass begun (or no room): the master does it now
		return;
	}
	jobs_given[jobs_count] = job;
	jobs_count = jobs_count + 1;
	jo_core_exec_on_slave(run_job); // queued after the meshes handed so far: the master doesn't wait
}

void slave_draw_mesh_here(const jo_3d_mesh *mesh)
{
	if (in_job)
		jo_3d_slave_mesh_draw(mesh);
	else
		jo_3d_mesh_draw((jo_3d_mesh *)mesh);
}

void slave_draw_end(void)
{
	if (!begun)
		return;
	jo_core_wait_for_slave(); // every mesh handed over is drawn
	jo_3d_slave_end();		  // the slave's commands join the master's for this frame
	begun = false;
}

#else

// Without jo's dual CPU module: the master draws everything, as it's handed over
void slave_draw_begin(void)
{
}

void slave_draw_mesh(const jo_3d_mesh *mesh)
{
	jo_3d_mesh_draw((jo_3d_mesh *)mesh);
}

void slave_draw_job(slave_draw_job_fn job)
{
	job();
}

void slave_draw_mesh_here(const jo_3d_mesh *mesh)
{
	jo_3d_mesh_draw((jo_3d_mesh *)mesh);
}

void slave_draw_end(void)
{
}

#endif
