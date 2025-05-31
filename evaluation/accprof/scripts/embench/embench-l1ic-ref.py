import plotly.express as px
import pandas as pd

# Sample data
df = pd.DataFrame(dict(
    value = 
		[31, 32, 169, 321, 38, 309, 30, 41, 6, 43, 46, 474, 499, 278, 74, 108, 61, 6, 29, 32, 45, 6,
		28, 29, 311, 332, 38, 317, 43, 34, 6, 47, 65, 441, 622, 13, 410, 494, 47, 6, 43, 31, 296, 37],
    variable =
		['aha-mont64', 'crc32', 'cubic', 'edn', 'huffbench', 'matmult-int', 'md5sum', 'minver', 'nbody', 'nettle-aes', 'nettle-sha256', 'nsichneu', 'picojpeg', 'primecount', 'qrduino', 'sglib-combined', 'slre', 'st', 'statemate', 'tarfind', 'ud', 'wikisort',
		'aha-mont64', 'crc32', 'cubic', 'edn', 'huffbench', 'matmult-int', 'md5sum', 'minver', 'nbody', 'nettle-aes', 'nettle-sha256', 'nsichneu', 'picojpeg', 'primecount', 'qrduino', 'sglib-combined', 'slre', 'st', 'statemate', 'tarfind', 'ud', 'wikisort'],
    group =
		['APF', 'APF', 'APF', 'APF', 'APF', 'APF', 'APF', 'APF', 'APF', 'APF', 'APF', 'APF', 'APF', 'APF', 'APF', 'APF', 'APF', 'APF', 'APF', 'APF', 'APF', 'APF',
        'APF-soft', 'APF-soft', 'APF-soft', 'APF-soft', 'APF-soft', 'APF-soft', 'APF-soft', 'APF-soft', 'APF-soft', 'APF-soft', 'APF-soft', 'APF-soft', 'APF-soft', 'APF-soft', 'APF-soft', 'APF-soft', 'APF-soft', 'APF-soft', 'APF-soft', 'APF-soft', 'APF-soft', 'APF-soft', ]))

# Use scatter_polar for plotting points without lines
fig = px.scatter_polar(df, r='value', theta='variable', color='group',
	symbol='group', symbol_map={'APF': 'cross', 'APF-soft': 'x'},
	color_discrete_map={'APF': '#000000', 'APF-soft': 'rgba(0, 0, 0, 0.4)'})

# Set title, centre title, remove legend from groups
fig.update_layout(title="Embench Suite - Level 1 Instruction Cache refills", title_x=0.5, legend_title_text='')

# Set the dimensions of the chart to be square
fig.update_traces(marker=dict(size=22)) 
# Set the dimensions of the chart to be square
fig.update_layout(height=1000, width=1200)

# Change the background to white and lines to black
fig.update_layout(
    polar=dict(
		bgcolor='rgba(0, 0, 0, 0)',
        radialaxis=dict(
          tickfont=dict(color='black',size=28),  
			    gridcolor='rgb(200, 200, 200)'
						),
        angularaxis=dict(
          # tickcolor='rgb(220, 220, 220)',
          tickfont=dict(color='black',size=28), 
        
          gridcolor='rgb(220, 220, 220)'
	      )
    ),
	title=dict(
		font=dict(color='black', size=36)
	),
	legend=dict(
		font=dict(color='black', size=32)
	)
)

fig.write_image(__file__.rstrip('.py').split('/')[-1] + ".png")
