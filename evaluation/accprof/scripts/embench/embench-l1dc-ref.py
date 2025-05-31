import plotly.express as px
import pandas as pd

# Sample data
df = pd.DataFrame(dict(
    value = 
		[4, 10, 3, 102, 92, 162, 11, 7, 0, 29, 8, 7, 5, 71, 6, 35, 11, 0, 5, 30, 5, 3,
		2, 5, 0, 77, 54, 175, 5, 1, 0, 32, 2, 4, 84, 0, 83, 152, 2, 0, 1, 24, 65, 10],
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
fig.update_layout(title="Embench Suite - Level 1 Data Cache refills", title_x=0.5, legend_title_text='')

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
